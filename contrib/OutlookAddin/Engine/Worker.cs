using System;
using System.IO;
using System.Net;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;
using Forms = System.Windows.Forms;
using Engine.GUI;
using Engine.Parser;

using Microsoft.Office.Interop.Outlook;
using Exception=System.Exception;
using Outlook = Microsoft.Office.Interop.Outlook;

namespace Engine
{
    public class Worker : IDisposable
    {
        private readonly Konfiguration config = null;
        private readonly Logger logger = null;
        private readonly Language language = null;

        private readonly Outlook.Application outlook = null;
        private MailParser mailparser = null;

        public Worker(Konfiguration cfg, Logger log, Language lang) : this(cfg, log, lang, new Outlook.Application())
        {}

        public Worker(Konfiguration cfg, Logger log, Language lang, Outlook.Application objOutlook)
        {
            Forms.Application.EnableVisualStyles();
            Forms.Application.DoEvents();

            this.config = cfg;
            this.logger = log;
            this.language = lang;

            this.outlook = objOutlook;

            this.mailparser = new MailParser(this.logger, this.outlook, this);
            this.mailparser.resetAutotraining(this.config.UseAutoTrain, this.config.AutoTraining_Spam, this.config.AutoTraining_Ham);

            this.config.SettingsChanged += config_SettingsChanged;
        }

        ~Worker()
        {
            this.Dispose();
        }

        public void Dispose()
        {
            if (this.mailparser != null)
            {
                this.mailparser = null;
            }
        }

        private void config_SettingsChanged()
        {
            // update mailparser settings
            this.logger.WriteLine("Resetting Autotraining-Folders...");
            this.mailparser.resetAutotraining(this.config.UseAutoTrain, this.config.AutoTraining_Spam, this.config.AutoTraining_Ham);
            this.logger.WriteLine("...done");
        }

        public void Run(Boolean isSpam)
        {
            // Boolean bOk = true;
            // int iSkipped = 0;

            if (this.ActiveExplorer == null)
            {
                this.logger.WriteLine("ActiveExplorer is NULL. I can't do anything.");

                // errForm ef = new errForm(this.language, "ActiveExplorer is NULL. I can't do anything.");
                // ef.ShowDialog();
                return;
            }

            if (this.ActiveExplorer.Selection == null)
            {
                this.logger.WriteLine("ActiveExplorer.Selection is NULL. I can't do anything.");

                // errForm ef = new errForm(this.language, "ActiveExplorer.Selection is NULL. I can't do anything.");
                // ef.ShowDialog();
                return;
            }

            Outlook.Selection sel = this.ActiveExplorer.Selection;
            this.logger.WriteLine("Selected: '" + sel.Count + "' items.");

            for (int iMails = sel.Count; iMails > 0; iMails--)
            {
                this.logger.WriteLine("No at Item " + iMails);

                try
                {
                    if ((sel[iMails] is Outlook.MailItem) == false)
                    {
                        this.logger.WriteLine("Item '" + iMails + "' is no MailItem, skipped.");

                        // just handle mailitems
                        // iSkipped++;
                        continue;
                    }

                    Outlook.MailItem mi = (Outlook.MailItem) sel[iMails];
                    String signature = this.mailparser.getDSpamID(mi);

                    /*
                    // TrainMail checks for empty Sig
                    if (signature == "")
                    {
                        // no sig found.
                        this.logger.WriteLine("signature is empty, skipping...");
                        continue;
                    }
                    */

                    // go an train it!
                    this.TrainMail(isSpam, mi, signature);
                }
                catch(System.Exception ex)
                {
                    this.logger.WriteError("Run(): " + ex.Message);
                    this.logger.WriteError(ex.StackTrace);
                }
            }
        }

        public void ShowInterface()
        {
            Forms.Application.EnableVisualStyles();
            Forms.Application.DoEvents();

            MainForm mf = new MainForm(this.config, this.logger, this.language, this.mailparser, this.outlook, this);
            mf.ShowDialog();
        }

        public void CheckForUpdate(Boolean silent)
        {
            try
            {
                if (this.config.Url_CheckForUpdate == "")
                {
                    this.logger.WriteLine("CheckForUpdate: DISABLED");
                    return;
                }

                WebClient wc = new WebClient();
                Stream s = wc.OpenRead(this.config.Url_CheckForUpdate);
                StreamReader sr = new StreamReader(s);
                String strVersion = sr.ReadToEnd();
                sr.Close();
                s.Close();

                if (int.Parse(strVersion.Replace(".", "")) > int.Parse(this.config.Version.Replace(".", "")))
                {
                    // there is a newer version
                    MessageBox.Show(this.language.getUpdateBox("Found"), "Check for Update", MessageBoxButtons.OK, MessageBoxIcon.Information);
                    this.logger.WriteLine("CheckForUpdate: there is a newer Version.");
                }
                else
                {
                    if (silent == false)
                    {
                        // no difference
                        MessageBox.Show(this.language.getUpdateBox("NotFound"), "Check for Update");
                    }
                    this.logger.WriteLine("CheckForUpdate: no newer Version found.");
                }
            }
            catch (Exception ex)
            {
                this.logger.WriteError("bt_checkForUpdate_Click(): " + ex.Message);
                this.logger.WriteError(ex.StackTrace);
                MessageBox.Show("Exception found, please send LOG", "EXCEPTION HANDLER");
            }

        }

        public void SayThankYou()
        {
            try
            {
                MailItem oMailItem = (MailItem)this.outlook.CreateItem(OlItemType.olMailItem);
                oMailItem.To = this.config.Address_ThankYou;
                oMailItem.Subject = "Thank You!";
                oMailItem.Body = "i'd like to say 'thank you' to you.";

                // oMailItem.Send(); 
                oMailItem.Display(false);

                Marshal.ReleaseComObject(oMailItem);
            }
            catch (Exception ex)
            {
                this.logger.WriteError("SayThankYou(): " + ex.Message);
                this.logger.WriteError(ex.StackTrace);
            }
        }

        public void SendLogToDeveloper()
        {
            try
            {
                String log = this.logger.Cycle();

                MailItem oMailItem = (MailItem) this.outlook.CreateItem(OlItemType.olMailItem);
                oMailItem.To = this.config.Address_LogGoesTo;
                oMailItem.Subject = "Here is my LOG";
                oMailItem.Body = "as requested, my logfile.";
                oMailItem.Attachments.Add(log, OlAttachmentType.olByValue, 1, log);

                // oMailItem.Send(); 
                oMailItem.Display(false);

                Marshal.ReleaseComObject(oMailItem);
            }
            catch( Exception ex )
            {
                this.logger.WriteError("SendLogToDeveloper(): " + ex.Message);
                this.logger.WriteError(ex.StackTrace);
            }
        }

        private Explorer ActiveExplorer
        {
            get { return (Explorer)this.outlook.GetType().InvokeMember("ActiveExplorer", BindingFlags.GetProperty, null, this.outlook, null); }
        }

        internal void TrainMail(Boolean isSpam, MailItem mi, String signature)
        {
            if ((signature == "") || (mi == null))
            {
                this.logger.WriteError("signature is empty or MailItem is null.");
                return;
            }

            try
            {
                // train with what?
                // options: history or forward
                if (this.config.DoTrainWithHistory)
                {
                    this.TrainWithHistory(isSpam, signature);
                }
                else
                {
                    this.TrainWithForward(isSpam, mi);
                }

                // after training, do what?
                // nothing, delete or move
                if (isSpam)
                {
                    this.AfterTraining_Spam(mi);
                }
                else
                {
                    this.AfterTraining_Ham(mi);
                }
            }
            catch (Exception ex)
            {
                this.logger.WriteError("TrainMail(): " + ex.Message);
                this.logger.WriteError(ex.StackTrace);
            }
        }

        private void AfterTraining_Spam(MailItem mi)
        {
            if (this.config.AfterTraining_Spam_DoDelete)
            {
                // delete mail
                mi.Delete();
            }
            else
            {
                // mark as read
                if (this.config.AfterTraining_Spam_MarkAsRead)
                {
                    mi.UnRead = false;
                    mi.Save();
                }
                
                // prefix
                if (this.config.AfterTraining_Spam_DoPrefix)
                {
                    mi.Subject = this.config.AfterTraining_Spam_PrefixWith + " " + mi.Subject;
                    mi.Save();
                }
                
                // move
                if (this.config.AfterTraining_Spam_DoMove)
                {
                    if (this.config.AfterTraining_Spam_MoveTo != "")
                    {
                        mi.Move(this.mailparser.getFolderByName(this.config.AfterTraining_Spam_MoveTo));
                    }
                }
                else
                {
                    // do nothing
                }
            }
        }

        private void AfterTraining_Ham(MailItem mi)
        {
            if (this.config.AfterTraining_Ham_DoDelete)
            {
                // delete mail
                mi.Delete();
            }
            else
            {
                // mark as read
                if (this.config.AfterTraining_Ham_MarkAsRead)
                {
                    mi.UnRead = false;
                    mi.Save();
                }

                // prefix - it's ham, so remove the prefix
                if (this.config.AfterTraining_Ham_DoPrefix)
                {
                    // mi.Subject = this.config.AfterTraining_Ham_PrefixWith + " " + mi.Subject;
                    mi.Subject = mi.Subject.Replace(this.config.AfterTraining_Ham_PrefixWith, "");
                    mi.Save();
                }

                // move
                if (this.config.AfterTraining_Ham_DoMove)
                {
                    if (this.config.AfterTraining_Ham_MoveTo != "")
                    {
                        mi.Move(this.mailparser.getFolderByName(this.config.AfterTraining_Ham_MoveTo));
                    }
                }
                else
                {
                    // do nothing
                }
            }
        }

        private void TrainWithHistory(Boolean isSpam, String signature)
        {
            DateTime dtBegin = DateTime.Now;

            String s = "";

            if (isSpam)
            {
                s = this.config.UrlForSpam + signature;
            }
            else
            {
                s = this.config.UrlForHam + signature;
            }

            this.logger.WriteLine("Send request to '" + s + "'....");
            HttpWebRequest loHttp = (HttpWebRequest)WebRequest.Create(s);
            loHttp.CookieContainer = new CookieContainer();

            loHttp.Credentials = new NetworkCredential(this.config.DSpamLogin, this.config.DSpamPassword);
            this.logger.WriteLine("Added credentials (" + this.config.DSpamLogin + ")");
            
            loHttp.Proxy = WebRequest.DefaultWebProxy;
            this.logger.WriteLine("Added proxy (" + WebRequest.DefaultWebProxy + ")");
            loHttp.Method = "GET";

            try
            {
                HttpWebResponse loWebResponse = (HttpWebResponse)loHttp.GetResponse();
                StreamReader sr = new StreamReader(loWebResponse.GetResponseStream(), Encoding.Default);
                sr.ReadToEnd();

                sr.Close();
                loWebResponse.Close();

            }
            catch (Exception ex0)
            {
                this.logger.WriteError("HttpWebRespons encountered exception: " + ex0.Message);
                this.logger.WriteError(ex0.StackTrace);
            }

            this.logger.WriteLine("done, request took " + (DateTime.Now - dtBegin).TotalSeconds + " seconds.");
        }

        private void TrainWithForward(Boolean isSpam, MailItem mi)
        {
            DateTime dtBegin = DateTime.Now;
            String s = "";

            if (isSpam)
            {
                s = this.config.ForwardForSpam;
            }
            else
            {
                s = this.config.ForwardForHam;
            }

            this.logger.WriteLine("send mail to '" + s + "'....");
            
            MailItem miF = mi.Forward();
            miF.Recipients.Add(s);
            miF.Recipients.ResolveAll();
            miF.Send();

            this.logger.WriteLine("done, forward took " + (DateTime.Now - dtBegin).TotalSeconds + " seconds.");
        }
    }
}
