/* read and write options
* to preferences
*/

var _elementIDs = ["spamaddress", "hamaddress", "spamaction", "hamaction", "PickFolder0", "PickFolder1", "spamfolder", "hamfolder"];
	
function init()
{
	var pref = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);

	// initialize the default window values...
	for( var i = 0; i < _elementIDs.length; i++ )
	{
		var elementID = _elementIDs[i];
		var element = document.getElementById(elementID);
		if (!element) break;
			var eltType = element.localName;
		try 
		{
			if (eltType == "radiogroup")
				element.selectedIndex = pref.getIntPref(element.getAttribute("prefstring"));
			else if (eltType == "checkbox")
				element.checked = pref.getBoolPref(element.getAttribute("prefstring"));
			else if (eltType == "textbox")
				element.value = pref.getCharPref(element.getAttribute("prefstring")) ;
			else if (eltType == "menulist")
				SetFolderPicker(pref.getComplexValue(element.getAttribute("prefstring"), Components.interfaces.nsISupportsString).data, elementID);
		} catch (ex) {
			if (eltType == "radiogroup")
				element.selectedIndex = 0;
		}
	}

}

 
function savePrefs()
{
	var pref = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
	var str = Components.classes["@mozilla.org/supports-string;1"].createInstance(Components.interfaces.nsISupportsString);

	for( var i = 0; i < _elementIDs.length; i++ )
	{
		var elementID = _elementIDs[i];

		var element = document.getElementById(elementID);
		if (!element) break;
			var eltType = element.localName;

		if (eltType == "radiogroup")
			pref.setIntPref(element.getAttribute("prefstring"), element.selectedIndex);
		else if (eltType == "checkbox")
			pref.setBoolPref(element.getAttribute("prefstring"), element.checked);
		else if (eltType == "textbox" && element.preftype == "int")
			pref.setIntPref(element.getAttribute("prefstring"), parseInt(element.value) );
		else if (eltType == "textbox")
			pref.setCharPref(element.getAttribute("prefstring"), element.value );
		else if (eltType == "menulist")
			if ((elementID == "PickFolder0") || (elementID == "PickFolder1")) {
				var str = Components.classes["@mozilla.org/supports-string;1"].createInstance(Components.interfaces.nsISupportsString);
				str.data = element.getAttribute("uri");
				pref.setComplexValue(element.getAttribute("prefstring"), Components.interfaces.nsISupportsString, str);
			}
	}
}


function setFolderPicker(val,pickerID,targetID,selection) {

	try {
		var picker = document.getElementById(pickerID);
	} catch(e) {}

	var uri = val;
	if (selection) {
		var pn = selection.parentNode;
		while (pn.id.indexOf('PickFolde') < 0) {
			pn = pn.parentNode;
		}
		if (pn.id.indexOf('PickFolde') > -1) {
			var picker = pn;
		}
		uri = selection.getAttribute('id');
		val = selection.getAttribute('value');
		if (uri == '' && val == '') return;
	}
	var msgfolder = GetMsgFolderFromUri(uri, true);
	if (msgfolder == null) {
		if (val == QMglobals.bundle.getString("treeItemNone") ) uri = '';
	} else {
		val = document.getElementById("bundle_messenger").getFormattedString("verboseFolderFormat", [msgfolder.name, msgfolder.server.prettyName]);
	}        
	try {
		picker.setAttribute("label",val);
		picker.setAttribute("uri",uri);
	} catch(e) {}
}

/*
        function savePrefsnction() {
        	var pref = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
        	for( var i = 0; i < QMglobals.items; i++ ) {
                var elementID = 'PickFolder' + i;
                var element = document.getElementById(elementID);
                var uri = element.getAttribute("uri")
                if (!element) break;
                pref.setCharPref('quickmove.qf' + i, uri);
                var element = document.getElementById("qmMR" + i);
                if (!element) break;
                pref.setBoolPref("quickmove.markread" + i, element.checked);
        	}
        	for( var i = 1; i < 11; i++ ) {
                var element = document.getElementById('gl' + i);
                pref.setCharPref('quickmove.label.group' + i, element.value);
        	}
            pref.setBoolPref(QMglobals.bundle.getString("debugpref"), document.getElementById("QMdebug").checked);
            var obSvc = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
            obSvc.notifyObservers(null, "update_QMsettings", null);
        },  
*/




    function setBox(myId, myValue) {
      document.getElementById(myId).setAttribute("disabled", !myValue);
    }

    function getBox(myId) {
      return document.getElementById(myId).getAttribute("focused")
    }

    function updateCheckSpam() {
//      setBox("startupLabel", getBox("spammove"));
      setBox("PickFolder0", getBox("spammove"));
    }

    function updateCheckHam() {
//      setBox("startupLabel", getBox("spammove"));
      setBox("PickFolder1", getBox("hammove"));
    }

    function doDefault() {
      var selected = theListbox.selectedIndex;
      if (selected < 0) {
        alert(alerts.getString("dspam.options.errordefault"));
      } else {
        var defaultLabel = document.getElementById("defaultServerLabel");
        var newDefault = theListbox.getItemAtIndex(selected);
        theListbox.insertItemAt(0, defaultLabel.getAttribute("value"), defaultAccount);
        defaultLabel.setAttribute("value", newDefault.getAttribute("label"));
        defaultAccount = newDefault.getAttribute("value");
        theListbox.removeItemAt(selected+1);
        theListbox.ensureIndexIsVisible(0);
        theListbox.selectedIndex = 0;
      }
    }

    function doUp() {
      var selected = theListbox.selectedIndex;
      if (selected < 0) {
        alert(alerts.getString("dspam.options.errormoveup"));
        return;
      } else if (selected > 0) {
        var moveMe = theListbox.removeItemAt(selected-1);
        if (moveMe) {
          if(selected >= theListbox.getRowCount()) {
            theListbox.appendItem(moveMe.getAttribute("label"), moveMe.getAttribute("value"));
          } else {
            theListbox.insertItemAt(selected, moveMe.getAttribute("label"), moveMe.getAttribute("value"));
          }
          if(selected-2 >= 0)
            theListbox.ensureIndexIsVisible(selected-2);
          theListbox.ensureIndexIsVisible(selected);
        }
      }
    }

    function doDown() {
      var selected = theListbox.selectedIndex;
      if (selected < 0) {
        alert(alerts.getString("dspam.options.errormovedown"));
        return;
      } else if (selected < theListbox.getRowCount()-1) {
        var moveMe = theListbox.removeItemAt(selected+1);
        if (moveMe) {
          theListbox.insertItemAt(selected, moveMe.getAttribute("label"), moveMe.getAttribute("value"));
          if(selected+2 < theListbox.getRowCount())
            theListbox.ensureIndexIsVisible(selected+2);
          theListbox.ensureIndexIsVisible(selected+1);
        }
      }
    }

    function openURL(aURL) {
      var uri = Components.classes["@mozilla.org/network/standard-url;1"].createInstance(Components.interfaces.nsIURI);
      uri.spec = aURL;
      var protocolSvc = Components.classes["@mozilla.org/uriloader/external-protocol-service;1"].getService(Components.interfaces.nsIExternalProtocolService);
      protocolSvc.loadUrl(uri);
    }

function chuonthisInitPrefs(branch) {
  return Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefService).getBranch(branch);
}

function chuonthisGetPrefString(prefs, prefname) {
  try {
    return prefs.getComplexValue(prefname, Components.interfaces.nsISupportsString).data;
  } catch (ex) {
    return "";
  }
}

function chuonthisSetPrefString(prefs, prefname, prefvalue) {
  var str = Components.classes["@mozilla.org/supports-string;1"].createInstance(Components.interfaces.nsISupportsString);
  str.data = prefvalue;
  prefs.setComplexValue(prefname, Components.interfaces.nsISupportsString, str);
}

function chuonthisGetPrefBool(prefs, prefname, defvalue) {
  try {
    return prefs.getBoolPref(prefname);
  } catch (ex) {
    return defvalue;
  }
}
