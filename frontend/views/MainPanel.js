var
  kind = require('enyo/kind'),
  Panel = require('moonstone/Panel'),
  FittableRows = require('layout/FittableRows'),
  FittableColumns = require('layout/FittableColumns'),
  BodyText = require('moonstone/BodyText'),
  Marquee = require('moonstone/Marquee'),
  LunaService = require('enyo-webos/LunaService'),
	Divider = require('moonstone/Divider'),
	Scroller = require('moonstone/Scroller'),
  Item = require('moonstone/Item'),
	ToggleItem = require('moonstone/ToggleItem'),
	Group = require('enyo/Group');

var servicePath = "/media/developer/apps/usr/palm/services/org.webosbrew.hyperion.ng.loader.service";
var autostartFilepath = servicePath + "/autostart.sh";
var linkPath = "/var/lib/webosbrew/init.d/90-start_hyperiond";
module.exports = kind({
  name: 'MainPanel',
  kind: Panel,
  title: 'Hyperion.NG',
  titleBelow: "Loader",
  headerType: 'medium',
  components: [
    {kind: FittableColumns, classes: 'enyo-center', fit: true, components: [
      {kind: Scroller, fit: true, components: [
        {classes: 'moon-hspacing', controlClasses: 'moon-12h', components: [
          {components: [
            // {kind: Divider, content: 'Toggle Items'},
            {kind: ToggleItem, name: "autostart", content: 'Autostart', checked: true, disabled: true, onchange: "autostartToggle"},
            {kind: Item, content: 'Start', ontap: "start"},
            {kind: Item, content: 'Stop', ontap: "stop"}
          ]},
        ]},
      ]},
    ]},
    {components: [
      {kind: Divider, content: 'Result'},
      {kind: BodyText, name: 'result', content: 'Nothing selected...'}
    ]},
    {kind: LunaService, name: 'statusCheck', service: 'luna://org.webosbrew.hbchannel.service', method: 'exec', onResponse: 'onStatusCheck', onError: 'onStatusCheck'},
    {kind: LunaService, name: 'exec', service: 'luna://org.webosbrew.hbchannel.service', method: 'exec', onResponse: 'onExec', onError: 'onExec'},
  ],

  bindings: [],

  create: function () {
    this.inherited(arguments);
    this.$.statusCheck.send({
      command: 'readlink ' + linkPath,
    });
  },

  start: function (command) {
    this.exec("luna-send -n 1 'luna://org.webosbrew.hyperion.ng.loader.service/start' '{}'");
  },
  stop: function (command) {
    this.exec("luna-send -n 1 'luna://org.webosbrew.hyperion.ng.loader.service/stop' '{}'");
  },
  exec: function (command) {
    console.info(command);
    this.$.result.set('content', 'Processing...');
    this.$.exec.send({
      command: command,
    });
  },

  onExec: function (sender, evt) {
    console.info(evt);
    if (evt.returnValue) {
      this.$.result.set('content', 'Success!<br />' + evt.stdoutString + evt.stderrString);
    } else {
      this.$.result.set('content', 'Failed: ' + evt.errorText + ' ' + evt.stdoutString + evt.stderrString);
    }
  },

  onStatusCheck: function (sender, evt) {
    console.info(sender, evt);
    // this.$.result.set('content', JSON.stringify(evt.data));
    this.$.autostart.set('disabled', false);
    this.$.autostart.set('checked', evt.stdoutString && evt.stdoutString.trim() == autostartFilepath);
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