var sendListener = {

onGetDraftFolderURI: function ( folderURI ) {},
onProgress: function ( msgID , progress , progressMax ) {},
onSendNotPerformed: function ( msgID , status ) {},
onStartSending: function ( msgID , msgSize ) {},
onStatus: function ( msgID , msg ) {},

onStopSending: function ( msgID , status , msg , returnFileSpec )
{
	DeleteJunkMail(this.action);
},

	action : 0

};

function MoveToFolder()
{

}


var fwdStatus = 0;

function dspamReportSpam(event) {

	var validDspamConfig = true;
	var pref = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);

	try {
		var spamaction = pref.getIntPref("dspam.spamaction");
	} catch(e) {
		spamaction = 0;
	}

	try {
		var spamaddress = pref.getCharPref("dspam.spamaddress");
	} catch(e) {
		validDspamConfig = false;
	}

	if (spamaction != 0) {
		try {
			var spamfolder = RDF.GetResource(pref.getCharPref("dspam.spamfolder")).QueryInterface(Components.interfaces.nsIMsgFolder);
		} catch (e) {
			validDspamConfig = false;
		}
		try {
			var hamfolder = RDF.GetResource(pref.getCharPref("dspam.hamfolder")).QueryInterface(Components.interfaces.nsIMsgFolder);
		} catch (e) {
			validDspamConfig = false;
		}
	}

	var folder = GetLoadedMsgFolder();
	var rootFolder = folder.rootFolder;
	var messageArray = GetSelectedMessages();
	var server;
	server = folder.server;
	var dbv = GetDBView();

	if (validDspamConfig == true) {

		var msgComposeService = Components.classes["@mozilla.org/messengercompose;1"].getService();

		msgComposeService = msgComposeService.QueryInterface(Components.interfaces.nsIMsgComposeService);

		messenger.SetWindow(window, msgWindow);

		if (messageArray && messageArray.length > 0) {
			uri = "";
			for (var i = 0; i < messageArray.length; ++i) {
				var messageUri = messageArray[i];
 
				var hdr = messenger.msgHdrFromURI(messageUri);
       
				folder = hdr.folder;
				if (folder) {
					server = folder.server;
				}
       
				var accountKey = hdr.accountKey;
				if (accountKey.length > 0) {
					var account = accountManager.getAccount(accountKey);
					if (account)
						server = account.incomingServer;
				}
 
				var fwdStatus = true;

				if (server) {
					try {
						msgComposeService.forwardMessage(spamaddress, hdr, msgWindow, server);
					} catch(e) {
						fwdStatus = false;
					}

					if ( fwdStatus ) {
						if (spamaction == 1) {
							var out = new Object();
							var trashFolder = rootFolder.getFoldersWithFlag(MSG_FOLDER_FLAG_TRASH, 1, out);
							if (trashFolder) {
								try {
									dbv.doCommandWithFolder(nsMsgViewCommandType.moveMessages, trashFolder);
								} catch(e) {
									alert("Error while moving message to Trash Folder.");
								}
							} else
								alert("Could not get Trash folder settings.\nMessage was not moved.");
							}

						if ((spamaction == 2) && (spamfolder != hamfolder)) {
							try {
									dbv.doCommandWithFolder(nsMsgViewCommandType.moveMessages, spamfolder);
							} catch(e) {
								alert("Error while moving message to Spam Folder.");
							}
						}
					} else {
						alert("la la la.");
					}
				} else {
					alert("Unknown Error. Please contact the plugin author.");
				}

			}

		} else
			alert("You need to select at least one message.");
	} else {
		alert("Please check your SPAM reporting options.");
	}

}


function dspamReportHam(event) {

	var validDspamConfig = true;
	var pref = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);

	try {
		var hamaction = pref.getIntPref("dspam.hamaction");
	} catch(e) {
		hamaction = 0;
	}

	try {
		var hamaddress = pref.getCharPref("dspam.hamaddress");
	} catch(e) {
		validDspamConfig = false;
	}

	if (hamaction != 0) {
		try {
			var hamfolder = RDF.GetResource(pref.getCharPref("dspam.hamfolder")).QueryInterface(Components.interfaces.nsIMsgFolder);
		} catch (e) {
			validDspamConfig = false;
		}
		try {
			var spamfolder = RDF.GetResource(pref.getCharPref("dspam.spamfolder")).QueryInterface(Components.interfaces.nsIMsgFolder);
		} catch (e) {
			validDspamConfig = false;
		}
	}


	var folder = GetLoadedMsgFolder();
	var messageArray = GetSelectedMessages();
	var server;
	server = folder.server;

	var dbv = GetDBView();

	if (validDspamConfig == true) {

		var msgComposeService = Components.classes["@mozilla.org/messengercompose;1"].getService();

		var count = 0;

		if (messageArray && messageArray.length > 0) {
			for (var i = 0; i < messageArray.length; ++i) {
				var messageService = messenger.messageServiceFromURI(messageArray[i]);
				var messageStream = Components.classes["@mozilla.org/network/sync-stream-listener;1"].createInstance().QueryInterface(Components.interfaces.nsIInputStream);
				var inputStream = Components.classes["@mozilla.org/scriptableinputstream;1"].createInstance().QueryInterface(Components.interfaces.nsIScriptableInputStream);
				inputStream.init(messageStream);
				try {
					messageService.streamMessage(messageArray[i],messageStream, msgWindow, null, false, null);
				} catch (ex) {
					alert("Unknown Error.");
				}

			}
		}

		msgComposeService = msgComposeService.QueryInterface(Components.interfaces.nsIMsgComposeService);

		messenger.SetWindow(window, msgWindow);

		if (messageArray && messageArray.length > 0) {
			for (var i = 0; i < messageArray.length; ++i) {
				var messageUri = messageArray[i];
	 			var hdr = messenger.msgHdrFromURI(messageUri);
				folder = hdr.folder;
				if (folder) {
					server = folder.server;
				}
       
				var accountKey = hdr.accountKey;
				if (accountKey.length > 0) {
					var account = accountManager.getAccount(accountKey);
					if (account)
						server = account.incomingServer;
				}
 
				if (server) {
					try {
						msgComposeService.forwardMessage(hamaddress, hdr, msgWindow, server);
					} catch(e) {
						alert("la la la.");
					}

					if ((spamfolder != hamfolder) && (hamaction == 1))
						try {
							dbv.doCommandWithFolder(nsMsgViewCommandType.moveMessages, hamfolder);
						} catch(e) {
							alert("Error while moving message to Ham Folder.");
						}

				} else {
					alert("Unknown Error.");
				}
	
			}
		} else
			alert("You need to select at least one message.");
	} else {
		alert("Please check your HAM reporting options.");
	}

}

