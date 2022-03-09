import 'core-js/stable';
import 'regenerator-runtime/runtime';

import path from 'path';
import child_process, { exec } from 'child_process';

// @ts-ignore
import { Promise } from 'bluebird';
import Service, { Message } from 'webos-service';
import {asyncWriteFile, asyncReadFile} from './adapter';

import rootAppInfo from '../appinfo.json';
import serviceInfo from './services.json';
import { makeError, makeSuccess } from './protocol';

const execAsync = Promise.promisify(exec);

const configFilePath = path.join(__dirname, 'config.json');
const binaryPath = path.join(__dirname, 'hyperiond');

const kHyperionNGPackageId = rootAppInfo.id;
const service = new Service(serviceInfo.id);

let childProcess: child_process.ChildProcess = null;
let restart = false;

type Config = {
  verbose: boolean,
  debug: boolean,
  autostart: boolean,
};

function isRoot() {
  return process.getuid() === 0;
}

async function readConfig() {
  // Default values...
  const config: Config = {
    verbose: false,
    debug: false,
    autostart: false,
  };

  try {
    Object.assign(config, (await asyncReadFile(configFilePath)).toJSON());
  } catch (err) {
    console.warn(err);
  }

  return config;
}

async function saveConfig(config: Config) {
  await asyncWriteFile(configFilePath, JSON.stringify(config));
}

function spawnHyperiond(activity: any, config: Config) {
  const options: string[] = [];

  if (config.debug)
    options.push('--debug');
  else if (config.verbose)
    options.push('--verbose');

  console.info('Spawning hyperiond', options);

  return new Promise((resolve, reject) => {
    // Early crash detection
    let fulfilled = false;
    let log = '';

    setTimeout(() => {
      if (!fulfilled) {
        fulfilled = true;
        resolve(log);
      }
    }, 1000);

    childProcess = child_process.spawn(binaryPath, options, {
      stdio: ['pipe', 'pipe', 'pipe'],
      env: {
        ...process.env,
        LD_LIBRARY_PATH: __dirname,
      },
    });
    childProcess.stdout.on('data', (data: any) => {
      if (!fulfilled) {
        log += data;
      }
    });
    childProcess.stderr.on('data', (data: any) => {
      if (!fulfilled) {
        log += data;
      }
    });
    childProcess.on('close', (code: any, signal: any) => {
      if (!fulfilled) {
        fulfilled = true;
        reject(new Error('Process failed early: ' + log));
      }

      if (!restart) {
        if (activity) {
          service.activityManager.complete(activity, () => {
            console.info('Activity completed');
          });
          childProcess = null;
        }
      }

      console.info('Child process stopped!', code, signal);
      setTimeout(async () => {
        if (restart) {
          asyncCall(service, 'luna://com.webos.notification/createToast', { sourceId: kHyperionNGPackageId, message: 'Hyperion.NG restarted' });
          spawnHyperiond(activity, config);
        }
      }, 1000);
    });
  });
}

function asyncCall<T extends Record<string, any>>(srv: Service, uri: string, args: Record<string, any>): Promise<T> {
  return new Promise((resolve, reject) => {
    srv.call(uri, args, ({ payload }) => {
      if (payload.returnValue) {
        resolve(payload as T);
      } else {
        reject(payload);
      }
    });
  });
}

/**
 * Thin wrapper that responds with a successful message or an error in case of a JS exception.
 */
function tryRespond<T extends Record<string, any>>(runner: (message: Message) => T) {
  return async (message: Message): Promise<void> => {
    try {
      const reply: T = await runner(message);
      message.respond(makeSuccess(reply));
    } catch (err) {
      message.respond(makeError(err.message));
    } finally {
      message.cancel({});
    }
  };
}

service.register(
  'setSettings',
  tryRespond(async (message) => {
    console.log('Set settings called!');
    const config = await readConfig();
    Object.assign(config, message.payload);
    await saveConfig(config);
    return config;
  }),
);

service.register(
  'getSettings',
  tryRespond(async (message) => {
    return {
      loaded: true,
      ...(await readConfig()),
    };
  }),
);

service.register(
  'isStarted',
  tryRespond(async (message) => {
    console.log('isStarted called!');
    return {
      isStarted: childProcess && childProcess.exitCode === null ? true : false,
    };
  }),
);

service.register(
  'start',
  tryRespond(async (message) => {
    console.log('Service start called!');
    if (childProcess) {
      return { status: 'Already running!' };
    }

    const config = await readConfig();

    const activity = await new Promise((resolve, reject) => {
      service.activityManager.create('hyperiond-background', resolve);
    });

    // We want to always restart the process if we are autostarting, regardless
    // of early crashes...
    if (message.payload.autostart) {
      asyncCall(service, 'luna://com.webos.notification/createToast', { sourceId: kHyperionNGPackageId, message: 'Hyperion.NG starting up' });
      restart = true;
    }
    const result = await spawnHyperiond(activity, config);
    restart = true;

    return { returnValue: true, result };
  }),
);

service.register(
  'stop',
  tryRespond(async (message) => {
    console.log('Service stop called!');
    if (!childProcess) {
      return { status: 'Already stopped' };
    }
    restart = false;
    childProcess.kill();
  }),
);