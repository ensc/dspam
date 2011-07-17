using System.Reflection;
using Engine;
using Microsoft.Office.Core;
using Microsoft.Office.Interop.Outlook;
using Outlook = Microsoft.Office.Interop.Outlook;

namespace OutlookInterface
{
	using System;
	using Extensibility;
	using System.Runtime.InteropServices;

	[GuidAttribute("3A7B6BEB-0AEA-4F06-BC4D-0FDC403C73F6"), ProgId("OutlookInterface.Connect")]
	public class Connect : Object, Extensibility.IDTExtensibility2
	{
        private Outlook.Application objOutlook;
        private object addInInstance;

        private CommandBarButton cB_Button_AsSpam;
        private CommandBarButton cB_Button_AsHam;
        private CommandBarButton cB_Button_DoConfig;
	    private CommandBarButton cB_Button_DoThankYou;
        private CommandBarButton cB_Button_DoSendLogToDeveloper;

	    private Logger logger = null;
	    private Language language = null;
	    private Konfiguration konfiguration = null;
	    private Worker worker = null;

        public Connect()
		{
            String tmpPath = System.IO.Path.GetTempPath();
            if (tmpPath.EndsWith(@"\") == false)
            {
                tmpPath += @"\";
            }
            tmpPath += "dspam-addin.txt";

            try
            {
                this.logger = new Logger(tmpPath);

                this.konfiguration = new Konfiguration(@"dspam_config.xml", this.logger);
                this.konfiguration.Load();

                this.language = new Language(this.konfiguration, this.logger);
                this.language.Load();
            }
            catch (Exception ex)
            {
                // errForm ef = new errForm(this.language, @"can't create file '" + tmpPath + "' for logging:" + Environment.NewLine + ex.Message + Environment.NewLine + ex.StackTrace);
            }
		}

        public void OnConnection(object application, Extensibility.ext_ConnectMode connectMode, object addInInst, ref System.Array custom)
		{
            this.logger.WriteLine("OnConnection: Begin");

            this.objOutlook = (Application)application;
            this.addInInstance = addInInst;

            if (connectMode != Extensibility.ext_ConnectMode.ext_cm_Startup)
            {
                OnStartupComplete(ref custom);
            }
            this.logger.WriteLine("OnConnection: End");
        }

        public void OnDisconnection(Extensibility.ext_DisconnectMode disconnectMode, ref System.Array custom)
		{
            this.logger.WriteLine("OnDisconnection: Begin");

            if (disconnectMode != Extensibility.ext_DisconnectMode.ext_dm_HostShutdown)
            {
                OnBeginShutdown(ref custom);
            }

            GC.WaitForPendingFinalizers();
            GC.Collect();

            try
            {
                this.objOutlook = null;
                this.addInInstance = null;

                GC.WaitForPendingFinalizers();
                GC.Collect();
            }
            catch (Exception ex)
            {
                this.logger.WriteLine("OnDisconnection (1): Exception");
                this.logger.WriteLine(ex.Message);
                this.logger.WriteLine(ex.StackTrace);
            }

            GC.WaitForPendingFinalizers();
            GC.Collect();

            this.logger.WriteLine("OnDisconnection: End");
            this.logger.Dispose();
        }

        public void OnAddInsUpdate(ref System.Array custom)
		{
            this.logger.WriteLine("OnAddInsUpdate: Begin");
            this.logger.WriteLine("OnAddInsUpdate: End");
        }

		public void OnStartupComplete(ref System.Array custom)
		{
            try
            {
                this.logger.WriteLine("OnStartupComplete: Begin");
                try
                {
                    this.worker = new Worker(this.konfiguration, this.logger, this.language, this.objOutlook);
                    this.worker.CheckForUpdate(true);
                }
                catch (Exception ex_worker)
                {
                    this.logger.WriteLine("OnStartupComplete (1): Exception");
                    this.logger.WriteLine(ex_worker.Message);
                    this.logger.WriteLine(ex_worker.StackTrace);

                    // MessageBox.Show("OnStartupComplete" + Environment.NewLine + y.Message + Environment.NewLine + y.StackTrace);
                }
                
                this.AddToolbar();

                GC.WaitForPendingFinalizers();
                GC.Collect();

                this.logger.WriteLine("OnStartupComplete: End");
            }
            catch (Exception ex)
            {
                this.logger.WriteLine("OnStartupComplete (2): Exception");
                this.logger.WriteLine(ex.Message);
                this.logger.WriteLine(ex.StackTrace);

                // errForm ef = new errForm(this.language, "OnStartupComplete" + Environment.NewLine + ex.Message + Environment.NewLine + ex.StackTrace);
                // ef.ShowDialog();
            }		
        }

		public void OnBeginShutdown(ref System.Array custom)
		{
            this.logger.WriteLine("OnBeginShutdown: Begin");
            this.logger.WriteLine("OnBeginShutdown: End");
		}

        private void onClick_AsSpam(CommandBarButton Ctrl, ref bool CancelDefault)
        {
            if (this.worker != null)
            {
                this.worker.Run(true);
            }
        }

        private void onClick_AsHam(CommandBarButton Ctrl, ref bool CancelDefault)
        {
            if (this.worker != null)
            {
                this.worker.Run(false);
            }
        }

        private void onClick_DoConfig(CommandBarButton Ctrl, ref bool CancelDefault)
        {
            if (this.worker != null)
            {
                this.worker.ShowInterface();
            }
        }

        private void onClick_DoThankYou(CommandBarButton Ctrl, ref bool CancelDefault)
        {
            if (this.worker != null)
            {
                this.worker.SayThankYou();
            }
        }

        private void onClick_DoSendLogToDeveloper(CommandBarButton Ctrl, ref bool CancelDefault)
        {
            if (this.worker != null)
            {
                this.worker.SendLogToDeveloper();
            }
        }

        private Outlook.Explorer ActiveExplorer
        {
            get { return (Outlook.Explorer)this.objOutlook.GetType().InvokeMember("ActiveExplorer", BindingFlags.GetProperty, null, this.objOutlook, null); }
        }

        private void AddToolbar()
        {
            CommandBar oCommandBar = null;
            // CommandBars oCommandBars = null;

            try
            {
                // oCommandBars = (CommandBars)this.ActiveExplorer.CommandBars;
                // oCommandBar = oCommandBars["DSPAM"];
                oCommandBar = this.ActiveExplorer.CommandBars ["DSPAM"];
                this.logger.WriteLine("AddToolbar: ToolBar exist, reassigning the buttons.");

                foreach (CommandBarButton cbb in oCommandBar.Controls)
                {
                    cbb.Delete(false);
                }

                this.logger.WriteLine("AddToolbar: old buttons deleted.");
                this.AddToolbarButtons(oCommandBar);
            }
            catch
            {
                this.logger.WriteLine("AddToolbar: ToolBar doesn't exist.");

                // toolbar doesnt exist or something got wrong
                try
                {
                    // create toolbar
                    // oCommandBar = oCommandBars.Add("DSPAM", MsoBarPosition.msoBarTop, false, false);
                    oCommandBar = this.ActiveExplorer.CommandBars.Add("DSPAM", MsoBarPosition.msoBarTop, false, false);
                    oCommandBar.Visible = true;

                    this.logger.WriteLine("ToolBar added");

                    this.AddToolbarButtons(oCommandBar);
                }
                catch (Exception ex_int)
                {
                    this.logger.WriteLine("AddToolbar : Exception");
                    this.logger.WriteLine(ex_int.Message);
                    this.logger.WriteLine(ex_int.StackTrace);

                    // errForm ef = new errForm(this.language, "AddToolbar" + Environment.NewLine + ex_int.Message + Environment.NewLine + ex_int.StackTrace);
                    // ef.ShowDialog();
                }
            }
        }
        
        private void AddToolbarButtons(CommandBar oCommandBar)
        {
            // add buttons
            this.logger.WriteLine("AddToolbarButtons: Adding Buttons.");

            this.cB_Button_AsSpam = (CommandBarButton)oCommandBar.Controls.Add(MsoControlType.msoControlButton, Missing.Value, Missing.Value, Missing.Value, false);
            this.cB_Button_AsSpam.Caption = this.language.getToolbarCaption("cB_Button_AsSpam");
            // "&Spam";
            this.cB_Button_AsSpam.Style = MsoButtonStyle.msoButtonCaption;
            this.cB_Button_AsSpam.Tag = this.language.getToolbarTag("cB_Button_AsSpam");
            // "this IS spam";
            this.cB_Button_AsSpam.Visible = true;
            this.cB_Button_AsSpam.Click += new _CommandBarButtonEvents_ClickEventHandler(this.onClick_AsSpam);
            this.logger.WriteLine("Button AsSpam added");

            this.cB_Button_AsHam = (CommandBarButton)oCommandBar.Controls.Add(MsoControlType.msoControlButton, Missing.Value, Missing.Value, Missing.Value, false);
            this.cB_Button_AsHam.Caption = this.language.getToolbarCaption("cB_Button_AsHam");
            // "&Ham";
            this.cB_Button_AsHam.Style = MsoButtonStyle.msoButtonCaption;
            this.cB_Button_AsHam.Tag = this.language.getToolbarTag("cB_Button_AsHam");
            // "this is NOT spam";
            this.cB_Button_AsHam.Visible = true;
            this.cB_Button_AsHam.Click += new _CommandBarButtonEvents_ClickEventHandler(this.onClick_AsHam);
            this.logger.WriteLine("Button AsHam added");

            this.cB_Button_DoConfig = (CommandBarButton)oCommandBar.Controls.Add(MsoControlType.msoControlButton, Missing.Value, Missing.Value, Missing.Value, false);
            this.cB_Button_DoConfig.Caption = this.language.getToolbarCaption("cB_Button_DoConfig");
            // "&Configure";
            this.cB_Button_DoConfig.Style = MsoButtonStyle.msoButtonCaption;
            this.cB_Button_DoConfig.Tag = this.language.getToolbarTag("cB_Button_DoConfig");
            // "Edit your Configuration";
            this.cB_Button_DoConfig.Visible = true;
            this.cB_Button_DoConfig.Click += new _CommandBarButtonEvents_ClickEventHandler(this.onClick_DoConfig);
            this.logger.WriteLine("Button DoConfig added");

            // this.bt_thankYou.Text = this.language.getMainForm("bt_thankYou");
            this.cB_Button_DoThankYou = (CommandBarButton)oCommandBar.Controls.Add(MsoControlType.msoControlButton, Missing.Value, Missing.Value, Missing.Value, false);
            this.cB_Button_DoThankYou.Caption = this.language.getMainForm("bt_thankYou");
            // "&Configure";
            this.cB_Button_DoThankYou.Style = MsoButtonStyle.msoButtonCaption;
            this.cB_Button_DoThankYou.Tag = this.language.getMainForm("bt_thankYou");
            // "Edit your Configuration";
            this.cB_Button_DoThankYou.Visible = true;
            this.cB_Button_DoThankYou.Click += new _CommandBarButtonEvents_ClickEventHandler(this.onClick_DoThankYou);
            this.cB_Button_DoThankYou.BeginGroup = true;
            this.logger.WriteLine("Button DoThankYou added");

            // this.bt_sendLogToDeveloper.Text = this.language.getMainForm("bt_sendLogToDeveloper");
            this.cB_Button_DoSendLogToDeveloper = (CommandBarButton)oCommandBar.Controls.Add(MsoControlType.msoControlButton, Missing.Value, Missing.Value, Missing.Value, false);
            this.cB_Button_DoSendLogToDeveloper.Caption = this.language.getMainForm("bt_sendLogToDeveloper");
            // "&Configure";
            this.cB_Button_DoSendLogToDeveloper.Style = MsoButtonStyle.msoButtonCaption;
            this.cB_Button_DoSendLogToDeveloper.Tag = this.language.getMainForm("bt_sendLogToDeveloper");
            // "Edit your Configuration";
            this.cB_Button_DoSendLogToDeveloper.Visible = true;
            this.cB_Button_DoSendLogToDeveloper.Click += new _CommandBarButtonEvents_ClickEventHandler(this.onClick_DoSendLogToDeveloper);
            this.logger.WriteLine("Button DoSendLogToDeveloper added");


            this.logger.WriteLine("AddToolbarButtons: All Buttons added.");
        }
	}
}