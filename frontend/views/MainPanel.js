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

var serviceName = "org.webosbrew.hyperhdr.loader.service";
var servicePath = "/media/developer/apps/usr/palm/services/" + serviceName;
var autostartFilepath = servicePath + "/autostart.sh";
var linkPath = "/var/lib/webosbrew/init.d/90-start_hyperhdr";
var elevationCommand = "/media/developer/apps/usr/palm/services/org.webosbrew.hbchannel.service/elevate-service " + serviceName;

var not = function (x) { return !x };
var yes_no_bool = function (x) {
  if (x)
    return "Yes";
  else
    return "No";
}

const sleep = (duration) => {
  return new Promise(resolve => setTimeout(resolve, duration));
}

module.exports = kind({
  name: 'MainPanel',
  kind: Panel,
  title: 'HyperHDR',
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
              label: 'HyperHDR version',
              name: 'hyperhdrVersion',
              text: 'unknown',
              disabled: true,
            },
            {kind: ToggleItem, name: "autostart", content: 'Autostart', checked: true, disabled: true, onchange: "autostartToggle"},
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
    {kind: LunaService, name: 'serviceStatus', service: 'luna://org.webosbrew.hyperhdr.loader.service', method: 'status', onResponse: 'onServiceStatus', onError: 'onServiceStatus'},
    {kind: LunaService, name: 'start', service: 'luna://org.webosbrew.hyperhdr.loader.service', method: 'start', onResponse: 'onDaemonStart', onError: 'onDaemonStart'},
    {kind: LunaService, name: 'stop', service: 'luna://org.webosbrew.hyperhdr.loader.service', method: 'stop', onResponse: 'onDaemonStop', onError: 'onDaemonStop'},
    {kind: LunaService, name: 'version', service: 'luna://org.webosbrew.hyperhdr.loader.service', method: 'version', onResponse: 'onDaemonVersion', onError: 'onDaemonVersion'},
    {kind: LunaService, name: 'terminate', service: 'luna://org.webosbrew.hyperhdr.loader.service', method: 'terminate', onResponse: 'onTermination', onError: 'onTermination'},
    {kind: LunaService, name: 'autostartStatusCheck', service: 'luna://org.webosbrew.hbchannel.service', method: 'exec', onResponse: 'onAutostartCheck', onError: 'onAutostartCheck'},

    {kind: LunaService, name: 'exec', service: 'luna://org.webosbrew.hbchannel.service', method: 'exec', onResponse: 'onExec', onError: 'onExec'},
    {kind: LunaService, name: 'systemReboot', service: 'luna://org.webosbrew.hbchannel.service', method: 'reboot'},
  ],

  autostartEnabled: false,
  serviceElevated: false,
  daemonRunning: false,
  hyperhdrVersionText: 'unknown',
  resultText: 'unknown',

  bindings: [
    {from: "autostartEnabled", to: '$.autostart.checked'},
    {from: "serviceElevated", to: '$.startButton.disabled', transform: not},
    {from: "serviceElevated", to: '$.stopButton.disabled', transform: not},
    {from: "serviceElevated", to: '$.autostart.disabled', transform: not},
    {from: "hyperhdrVersionText", to: '$.hyperhdrVersion.text'},
    {from: "serviceElevated", to: '$.elevationStatus.text', transform: yes_no_bool},
    {from: "daemonRunning", to: '$.daemonStatus.text', transform: yes_no_bool},
    {from: "resultText", to: '$.result.content'}
  ],

  create: function () {
    this.inherited(arguments);
    console.info("Application created");
    // At first, elevate the native service
    // It does not do harm if service is elevated already
    this.elevate();
    this.set('resultText','Checking service status...');
    this.$.serviceStatus.send({});
  },
  // Elevates the native service - this enables hyperhdr.loader.service to run as root by default
  elevate: function () {
    console.info("Sending elevation command");
    this.$.exec.send({command: elevationCommand});
  },
  // Kill the loader service. Needed to force-restart after elevation
  terminate: function() {
    console.info("Sending termination command");
    this.$.terminate.send({});
  },
  reboot: function () {
    console.info("Sending reboot command");
    this.$.systemReboot.send({reason: 'SwDownload'});
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
    this.set('serviceElevated', evt.elevated);
    this.set('daemonRunning', evt.running);
    this.set('resultText', 'Service status received..');
    if (this.serviceElevated) {
      this.checkAutostart();
      this.fetchVersion();
    } else {
      this.set('resultText', 'Restarting native service to elevate...');
      this.terminate();
    }
  },
  onAutostartCheck: function (sender, evt) {
    console.info("onAutostartCheck");
    console.info(sender, evt);
    // this.$.result.set('content', JSON.stringify(evt.data));
    this.set('autostartEnabled', (evt.returnValue && evt.stdoutString && evt.stdoutString.trim() == autostartFilepath));
    this.set('resultText', 'Autostart check completed');
  },
  onTermination: function (sender, evt) {
    console.info("onTermination");
    this.set('resultText',"Native service terminated - Please restart the app!");
  },
  onDaemonStart: function (sender, evt) {
    console.info("onDaemonStart");
    if (evt.returnValue) {
      this.set('daemonRunning', true);
      this.set('resultText', "HyperHDR started");
    } else {
      this.set('resultText', "HyperHDR failed to start");
    }
  },
  onDaemonStop: function (sender, evt) {
    console.info("onDaemonStop");
    if (evt.returnValue) {
      this.set('daemonRunning', false);
      this.set('resultText', "HyperHDR stopped");
    } else {
      this.set('resultText', "HyperHDR failed to stop");
    }
  },
  onDaemonVersion: function (sender, evt) {
    console.info("onDaemonVersion");
    console.info(evt);
    if (evt.returnValue) {
      this.set('hyperhdrVersionText', evt.version);
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