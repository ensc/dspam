using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Net;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using Engine.Parser;
using Microsoft.Office.Interop.Outlook;
using Application=System.Windows.Forms.Application;
using Exception = System.Exception;
using Outlook = Microsoft.Office.Interop.Outlook;

namespace Engine.GUI
{
    public partial class MainForm : Form
    {
        private Konfiguration config = null;
        private Logger logger = null;
        private Language language = null;
        private MailParser mailparser = null;
        private Worker worker = null;

        private Outlook.Application outlook = null;
        
        public MainForm(Konfiguration cfg, Logger log, Language lang, MailParser mp, Outlook.Application objOutlook, Worker w)
        {
            InitializeComponent();

            this.worker = w;
            this.outlook = objOutlook;

            this.mailparser = mp;

            this.config = cfg;
            this.loadFromConfig();

            this.logger = log;

            this.language = lang;
            this.loadFromLanguage();

            this.bt_sendLogToDeveloper.Visible = false;
            this.bt_thankYou.Visible = false;
        }

        private void loadFromConfig()
        {
            this.tB_advertising.Text = this.config.Text_Advertising;
        }

        private void saveToConfig()
        {
        }

        private void loadFromLanguage()
        {
            // Buttons
            this.bt_thankYou.Text = this.language.getMainForm("bt_thankYou");
            this.bt_checkForUpdate.Text = this.language.getMainForm("bt_checkForUpdate");
            this.bt_sendLogToDeveloper.Text = this.language.getMainForm("bt_sendLogToDeveloper");
            this.bt_TrainingForm.Text = this.language.getMainForm("bt_TrainingForm");
            this.bt_AktionForm.Text = this.language.getMainForm("bt_AktionForm");
            this.bt_closeForm.Text = this.language.getMainForm("bt_closeForm");
        }

        private void bt_thankYou_Click(object sender, EventArgs e)
        {
            // System.Threading.Thread t = new Thread(new ThreadStart(this.threaded_ThankYou));
            // t.Start();

            this.Close();
            Application.DoEvents();

            Thread.Sleep(1000 * 2);

                // hier passierts
                this.logger.WriteLine("bt_thankYou_Click");
                Thread.Sleep(1000 * 2);

                MailItem oMailItem = (MailItem)this.outlook.CreateItem(OlItemType.olMailItem);
                oMailItem.To = this.config.Address_ThankYou;
                oMailItem.Subject = "Thank You!";
                oMailItem.Body = "i'd like to say 'thank you' to you.";

                // oMailItem.Send(); 
                oMailItem.Display(false);
                Marshal.ReleaseComObject(oMailItem);
        }

        private void threaded_ThankYou()
        {
            try
            {
                Thread.Sleep(1000*2);

                MailItem oMailItem = (MailItem) this.outlook.CreateItem(OlItemType.olMailItem);
                oMailItem.To = this.config.Address_ThankYou;
                oMailItem.Subject = "Thank You!";
                oMailItem.Body = "i'd like to say 'thank you' to you.";

                // oMailItem.Send(); 
                oMailItem.Display(false);

                oMailItem = null;
                Marshal.ReleaseComObject(oMailItem);
            }
            catch (Exception ex)
            {
                this.logger.WriteError("threaded_ThankYou(): " + ex.Message);
                this.logger.WriteError(ex.StackTrace);
            }
        }

        private void bt_checkForUpdate_Click(object sender, EventArgs e)
        {
            this.worker.CheckForUpdate(false);
        }

        private void bt_sendLogToDeveloper_Click(object sender, EventArgs e)
        {
            System.Threading.Thread t = new Thread(new ThreadStart(this.threaded_LogToDeveloper));
            t.Start();
            this.Close();
        }

        private void threaded_LogToDeveloper()
        {
            try
            {
                Thread.Sleep(1000*2);
                String log = this.logger.Cycle();

                MailItem oMailItem = (MailItem) this.outlook.CreateItem(OlItemType.olMailItem);
                oMailItem.To = this.config.Address_LogGoesTo;
                oMailItem.Subject = "Here is my LOG";
                oMailItem.Body = "as requested, my logfile.";
                oMailItem.Attachments.Add(log, OlAttachmentType.olByValue, 1, log);

                // oMailItem.Send(); 
                oMailItem.Display(false);

                oMailItem = null;
                Marshal.ReleaseComObject(oMailItem);
            }
            catch (Exception ex)
            {
                this.logger.WriteError("threaded_LogToDeveloper(): " + ex.Message);
                this.logger.WriteError(ex.StackTrace);
            }
        }

        private void bt_closeForm_Click(object sender, EventArgs e)
        {
            this.Close();
        }

        private void bt_TrainingForm_Click(object sender, EventArgs e)
        {
            TrainingForm tf = new TrainingForm(this.config, this.logger, this.language, this.mailparser);
            tf.ShowDialog();
        }

        private void bt_AktionForm_Click(object sender, EventArgs e)
        {
            AktionForm af  = new AktionForm(this.config, this.logger, this.language, this.mailparser);
            af.ShowDialog();
        }
    }
}