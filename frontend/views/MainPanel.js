var
  kind = require('enyo/kind'),
  Panel = require('moonstone/Panel'),
  FittableColumns = require('layout/FittableColumns'),
  BodyText = require('moonstone/BodyText'),
  LunaService = require('enyo-webos/LunaService'),
	Divider = require('moonstone/Divider'),
	Scroller = require('moonstone/Scroller'),
  Item = require('moonstone/Item'),
	ToggleItem = require('moonstone/ToggleItem'),
  LabeledTextItem = require('moonstone/LabeledTextItem');

var daemonName = "HyperHDR";
var serviceName = "org.webosbrew.hyperhdr.loader.service";
var lunaServiceUri = "luna://" + serviceName;
var servicePath = "/media/developer/apps/usr/palm/services/" + serviceName;
var autostartFilepath = servicePath + "/autostart.sh";
var linkPath = "/var/lib/webosbrew/init.d/90-start_hyperhdr";
var elevationCommand = "/media/developer/apps/usr/palm/services/org.webosbrew.hbchannel.service/elevate-service " + serviceName + " && killall -9 loader_service";

var not = function (x) { return !x };
var yes_no_bool = function (x) {
  if (x)
    return "Yes";
  else
    return "No";
}

module.exports = kind({
  name: 'MainPanel',
  kind: Panel,
  title: daemonName,
  titleBelow: "Loader",
  headerType: 'medium',
  components: [
    {kind: FittableColumns, classes: 'enyo-center', fit: true, components: [
      {kind: Scroller, fit: true, components: [
        {classes: 'moon-hspacing', controlClasses: 'moon-12h', components: [
          {components: [
            // {kind: Divider, content: 'Toggle Items'},
            {
              kind: LabeledTextItem,
              label: 'Service elevated',
              name: 'elevationStatus',
              text: 'unknown',
              disabled: true,
            },
            {
              kind: LabeledTextItem,
              label: 'Daemon running',
              name: 'daemonStatus',
              text: 'unknown',
              disabled: true,
            },
            {
              kind: LabeledTextItem,
              label: 'Daemon version',
              name: 'daemonVersion',
              text: 'unknown',
              disabled: true,
            },
            {kind: ToggleItem, name: "autostart", content: 'Autostart', checked: false, disabled: true, onchange: "autostartToggle"},
            {kind: Item, name: 'startButton', content: 'Start', ontap: "start", disabled: true},
            {kind: Item, name: 'stopButton', content: 'Stop', ontap: "stop", disabled: true},
            {kind: Item, name: 'rebootButton', content: 'Reboot system', ontap: "reboot"},
          ]},
        ]},
      ]},
    ]},
    {components: [
      {kind: Divider, content: 'Result'},
      {kind: BodyText, name: 'result', content: 'Nothing selected...'}
    ]},
    {kind: LunaService, name: 'serviceStatus', service: lunaServiceUri, method: 'status', onResponse: 'onServiceStatus', onError: 'onServiceStatus'},
    {kind: LunaService, name: 'start', service: lunaServiceUri, method: 'start', onResponse: 'onDaemonStart', onError: 'onDaemonStart'},
    {kind: LunaService, name: 'stop', service: lunaServiceUri, method: 'stop', onResponse: 'onDaemonStop', onError: 'onDaemonStop'},
    {kind: LunaService, name: 'version', service: lunaServiceUri, method: 'version', onResponse: 'onDaemonVersion', onError: 'onDaemonVersion'},
    {kind: LunaService, name: 'terminate', service: lunaServiceUri, method: 'terminate', onResponse: 'onTermination', onError: 'onTermination'},

    {kind: LunaService, name: 'autostartStatusCheck', service: 'luna://org.webosbrew.hbchannel.service', method: 'exec', onResponse: 'onAutostartCheck', onError: 'onAutostartCheck'},
    {kind: LunaService, name: 'exec', service: 'luna://org.webosbrew.hbchannel.service', method: 'exec', onResponse: 'onExec', onError: 'onExec'},
    {kind: LunaService, name: 'execSilent', service: 'luna://org.webosbrew.hbchannel.service', method: 'exec'},
    {kind: LunaService, name: 'systemReboot', service: 'luna://org.webosbrew.hbchannel.service', method: 'reboot' }
  ],

  autostartEnabled: false,
  serviceElevated: false,
  daemonRunning: false,
  daemonVersionText: 'unknown',
  resultText: 'unknown',

  initDone: false,
  autostartStatusChecked: false,

  bindings: [
    {from: "autostartStatusChecked", to: '$.autostart.disabled', transform: not},
    {from: "autostartEnabled", to: '$.autostart.checked'},

    {from: "serviceElevated", to: '$.startButton.disabled', transform: not},
    {from: "serviceElevated", to: '$.stopButton.disabled', transform: not},
    {from: "daemonVersionText", to: '$.daemonVersion.text'},
    {from: "serviceElevated", to: '$.elevationStatus.text', transform: yes_no_bool},
    {from: "daemonRunning", to: '$.daemonStatus.text', transform: yes_no_bool},
    {from: "resultText", to: '$.result.content'}
  ],

  create: function () {
    this.inherited(arguments);
    console.info("Application created");
    this.doStartup();
  },
  // Spawned from this.create()
  doStartup: function() {
    console.info('doStartup');
    this.checkAutostart();
    this.set('resultText', 'Waiting for service status data...');
    var self = this;
    // Start to continuosly poll service status
    setInterval(function () {
      self.$.serviceStatus.send({});
    }, 2000);
  },
  // Elevates the native service - this enables it to run as root by default
  elevate: function () {
    console.info("Sending elevation command");
    this.$.execSilent.send({command: elevationCommand});
  },
  // Kill the loader service. Needed to force-restart after elevation
  terminate: function() {
    console.info("Sending termination command");
    this.$.terminate.send({});
  },
  reboot: function () {
    console.info("Sending reboot command");
    this.$.systemReboot.send({});
  },
  start: function () {
    console.info("Start clicked");
    this.$.start.send({});
  },
  stop: function () {
    console.info("Stop clicked");
    this.$.stop.send({});
  },
  fetchVersion: function () {
    console.info("Fetch version called");
    this.$.version.send({});
  },
  checkAutostart: function () {
    console.info("checkAutostart");
    this.$.autostartStatusCheck.send({
      command: 'readlink ' + linkPath,
    });
  },
  exec: function (command) {
    console.info("exec called");
    console.info(command);
    this.set('resultText','Processing...');
    this.$.exec.send({
      command: command,
    });
  },
  onExec: function (sender, evt) {
    console.info("onExec");
    console.info(evt);
    if (evt.returnValue) {
      this.set('resultText','Success!<br />' + evt.stdoutString + evt.stderrString);
    } else {
      this.set('resultText','Failed: ' + evt.errorText + ' ' + evt.stdoutString + evt.stderrString);
    }
  },
  onServiceStatus: function (sender, evt) {
    console.info("onServiceStatus");
    console.info(sender, evt);

    if (!evt.returnValue) {
      console.info('Service status call failed!');
      return;
    }

    this.set('serviceElevated', evt.elevated);
    this.set('daemonRunning', evt.running);
    if (this.serviceElevated && !this.initDone) {
      this.set('resultText', 'Startup routine finished!');
      this.initDone = true;
      this.fetchVersion();
    } else if (!this.serviceElevated) {
      // Elevate the native service
      // Eventually the next service status callback will report elevation
      this.set('resultText', 'Trying to elevate...');
      this.elevate();
    }
  },
  onAutostartCheck: function (sender, evt) {
    console.info("onAutostartCheck");
    console.info(sender, evt);
    // this.$.result.set('content', JSON.stringify(evt.data));
    this.set('autostartEnabled', (evt.returnValue && evt.stdoutString && evt.stdoutString.trim() == autostartFilepath));
    this.set('autostartStatusChecked', true);
  },
  onTermination: function (sender, evt) {
    console.info("onTermination");
    this.set('resultText',"Native service terminated - Please restart the app!");
  },
  onDaemonStart: function (sender, evt) {
    console.info("onDaemonStart");
    if (evt.returnValue) {
      this.set('daemonRunning', true);
      this.set('resultText', "Daemon started");
    } else {
      this.set('resultText', "Daemon failed to start");
    }
  },
  onDaemonStop: function (sender, evt) {
    console.info("onDaemonStop");
    if (evt.returnValue) {
      this.set('daemonRunning', false);
      this.set('resultText', "Daemon stopped");
    } else {
      this.set('resultText', "Daemon failed to stop");
    }
  },
  onDaemonVersion: function (sender, evt) {
    console.info("onDaemonVersion");
    console.info(evt);
    if (evt.returnValue) {
      this.set('daemonVersionText', evt.version);
    }
  },
  autostartToggle: function (sender) {
    console.info("toggle:", sender);

    if (sender.active) {
      this.exec('mkdir -p /var/lib/webosbrew/init.d && ln -sf ' + autostartFilepath + ' ' + linkPath);
    } else {
      this.exec('rm -rf ' + linkPath);
    }
  },
});