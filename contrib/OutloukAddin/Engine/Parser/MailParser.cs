using System;
using System.Collections;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using MAPIPROPLib;
using Microsoft.Office.Interop.Outlook;
using Exception = System.Exception;
using Outlook = Microsoft.Office.Interop.Outlook;

namespace Engine.Parser
{
    public class MailParser : IDisposable
    {
        // private static int iSleepTime = 1000 * 5;

        private static int CdoPR_TRANSPORT_MESSAGE_HEADERS = 0x7D001E; // 8192030
        private static int CdoPR_BODY = 0x1000001E; // 268435486

        private Logger logger = null;
        
        private Outlook.Application outlook = null;
        private Outlook.NameSpace ns = null;

        private MAPIPROPLib.MAPIPropWrapper wrapper = null;

        private Worker worker = null;

        private MAPIFolder folder_spam = null;
        private Outlook.Items items_spam = null;

        private MAPIFolder folder_ham = null;
        private Outlook.Items items_ham = null;

        /*
        private Queue queue_spam = null;
        private Queue queue_ham = null;

        private Thread thread_spam = null;
        private Thread thread_ham = null;
        */
        public MailParser(Logger log, Outlook.Application objOutlook, Worker w)
        {
            this.logger = log;
            this.outlook = objOutlook;

            this.ns = this.outlook.GetNamespace("MAPI");
            this.ns.Logon(Missing.Value, Missing.Value, false, false);

            this.wrapper = new MAPIPropWrapper();
            this.wrapper.Initialize();

            this.worker = w;

            /*
            this.queue_spam = new Queue();
            this.queue_ham = new Queue();

            this.thread_spam = new Thread(new ThreadStart(this.handleSpam));
            // this.thread_spam.Start();

            this.thread_ham = new Thread(new ThreadStart(this.handleHam));
            // this.thread_ham.Start();
            */
        }

        public MailParser(Logger log, Worker w) : this(log, new Outlook.Application(), w)
        {
        }

        ~MailParser()
        {
            this.Dispose();
        }

        public void Dispose()
        {
            try
            {
                this.handleAutotrain(false);

                /*
                if ((this.thread_spam != null) && (this.thread_spam.IsAlive))
                {
                    this.thread_spam.Abort();
                    this.thread_spam = null;
                }

                if ((this.thread_ham != null) && (this.thread_ham.IsAlive))
                {
                    this.thread_ham.Abort();
                    this.thread_ham = null;
                }
                */

                if (this.wrapper != null)
                {
                    this.wrapper.Uninitialize();
                    this.wrapper = null;
                }

                if (this.ns != null)
                {
                    this.ns.Logoff();
                    this.ns = null;
                }
            }
            catch (Exception ex)
            {
                this.logger.WriteError("MailParser.Dispose() " + ex.Message);
                this.logger.WriteError(ex.StackTrace);
            }
        }

        /*
        public void Run(Outlook.MailItem mi)
        {
            String hdr = this.wrapper.GetOneProp(mi, CdoPR_TRANSPORT_MESSAGE_HEADERS).ToString();
            String bdy = this.wrapper.GetOneProp(mi, CdoPR_BODY).ToString();

            Console.WriteLine(hdr);
            Console.WriteLine(bdy);
        }
        */

        public void resetAutotraining(Boolean enable, String new_folder_spam, String new_folder_ham)
        {
            try
            {
                this.logger.WriteLine("Resetting AutoTrain, enabled = " + enable);
                if (enable)
                {
                    this.handleAutotrain(false);

                    this.folder_spam = this.getFolderByName(new_folder_spam);
                    this.folder_ham = this.getFolderByName(new_folder_ham);

                    this.handleAutotrain(true);
                }
                else
                {
                    this.handleAutotrain(false);
                }
            }
            catch (Exception ex)
            {
                this.logger.WriteError("resetAutotraining(" + enable + ", " + new_folder_spam + ", " + new_folder_ham + "): " + ex.Message);
                this.logger.WriteError(ex.StackTrace);
            }
        }

        private void handleAutotrain(Boolean register)
        {
            if (register)
            {
                if (this.folder_spam != null)
                {
                    this.items_spam = this.folder_spam.Items;
                    this.items_spam.ItemAdd += new Outlook.ItemsEvents_ItemAddEventHandler(onNewItem_Spam);
                }

                if (this.folder_ham != null)
                {
                    this.items_ham = this.folder_ham.Items;
                    this.items_ham.ItemAdd += new Outlook.ItemsEvents_ItemAddEventHandler(onNewItem_Ham);
                }
            }
            else
            {
                if (this.items_spam != null)
                {
                    this.items_spam.ItemAdd -= new Outlook.ItemsEvents_ItemAddEventHandler(onNewItem_Spam);
                    this.items_spam = null;
                }

                if (this.items_ham != null)
                {
                    this.items_ham.ItemAdd -= new Outlook.ItemsEvents_ItemAddEventHandler(onNewItem_Ham);
                    this.items_ham = null;
                }
            }
        }

        private void onNewItem_Spam(object Item)
        {
            if (Item is Outlook.MailItem)
            {
                // Outlook.MailItem mi = (Outlook.MailItem)Item;
                this.logger.WriteLine("onNewItem_Spam: Enqueue");
                // this.queue_spam.Enqueue(Item);

                // ThreadPool.QueueUserWorkItem(this.callback_spam, Item);
                ThreadPool.QueueUserWorkItem(delegate(Object mi)
                {
                    // hier passierts
                    this.logger.WriteLine("QueueUserWorkItem_Spam");
                    this.worker.TrainMail(true, (Outlook.MailItem)mi, this.getDSpamID((Outlook.MailItem)mi));
                }, Item);
            }
        }

        private void onNewItem_Ham(object Item)
        {
            if (Item is Outlook.MailItem)
            {
                // Outlook.MailItem mi = (Outlook.MailItem)Item;
                this.logger.WriteLine("onNewItem_Ham: Enqueue");
                // this.queue_ham.Enqueue(Item);

                // ThreadPool.QueueUserWorkItem(this.callback_ham, Item);
                ThreadPool.QueueUserWorkItem(delegate(Object mi)
                {
                    // hier passierts
                    this.logger.WriteLine("QueueUserWorkItem_Ham");
                    this.worker.TrainMail(false, (Outlook.MailItem)mi, this.getDSpamID((Outlook.MailItem)mi));
                }, Item);
            }
        }

        /*
        private void handleSpam()
        {
            while (true)
            {
                this.logger.WriteLine("handleSpam: generatic Synchronized Queue");
                Queue q = Queue.Synchronized(this.queue_spam);

                if (q.Count <= 0)
                {
                    this.logger.WriteLine("handleSpam: Queue is empty.");
                    Thread.Sleep(iSleepTime);
                }
                else
                {
                    this.logger.WriteLine("handleSpam: Dequeuing");
                    while (q.Count > 0)
                    {
                        MailItem mi = (MailItem) q.Dequeue();
                        this.worker.TrainMail(true, mi, this.getDSpamID(mi));
                        Marshal.ReleaseComObject(mi);
                    }
                    this.logger.WriteLine("handleSpam: Dequeuing ended");
                }
            }
        }

        private void handleHam()
        {
            while (true)
            {
                this.logger.WriteLine("handleHam: generatic Synchronized Queue");
                Queue q = Queue.Synchronized(this.queue_spam);

                if (q.Count <= 0)
                {
                    this.logger.WriteLine("handleHam: Queue is empty.");
                    Thread.Sleep(iSleepTime);
                }
                else
                {
                    this.logger.WriteLine("handleHam: Dequeuing");
                    while (q.Count > 0)
                    {
                        MailItem mi = (MailItem) q.Dequeue();
                        this.worker.TrainMail(false, mi, this.getDSpamID(mi));
                        Marshal.ReleaseComObject(mi);
                    }
                    this.logger.WriteLine("handleHam: Dequeuing ended");
                }
            }
        }
        */

        public MAPIFolder getFolderByName(String folder)
        {
            if (folder == "")
            {
                return null;
            }

            Outlook.Folders oFolders = this.ns.Folders;
            MAPIFolder oFolder = null;

            if (folder.StartsWith(@"\\"))
            {
                folder = folder.Substring(2);
            }

            String[] fld = folder.Split('\\');

            try
            {
                for (int i = 0; i < fld.Length; i++)
                {
                    // Console.WriteLine(fld[i]);

                    if (i == 0)
                    {
                        oFolder = oFolders[fld[i]];
                    }
                    else
                    {
                        if (oFolder != null)
                        {
                            oFolder = oFolder.Folders[fld[i]];
                        }
                    }
                }
            }
            catch( Exception ex)
            {
                this.logger.WriteError("getFolderByName(" + folder + "): " + ex.Message);
                this.logger.WriteError(ex.StackTrace);
            }

            // Console.WriteLine("=> " + oFolder.FullFolderPath);
            return oFolder;
        }

        public String pickFolder()
        {
            MAPIFolder mf = this.ns.PickFolder();

            if (mf != null)
            {
                return mf.FullFolderPath;
            }
            else
            {
                return "";
            }

        }

        public String getDSpamID(Outlook.MailItem mi)
        {
            try
            {
                String hdr = this.wrapper.GetOneProp(mi, CdoPR_TRANSPORT_MESSAGE_HEADERS).ToString();
                String bdy = this.wrapper.GetOneProp(mi, CdoPR_BODY).ToString();

                String sig = "";
                int iSignaturePos = -1;

                iSignaturePos = bdy.IndexOf("!DSPAM:");
                if (iSignaturePos >= 0)
                {
                    // sig is in body
                    sig = bdy.Substring(iSignaturePos);
                    sig = sig.Substring(0, sig.LastIndexOf("!"));
                    sig = sig.Substring(7);

                    this.logger.WriteLine("Found sig in Body: " + sig);
                }
                else
                {
                    iSignaturePos = hdr.IndexOf("X-DSPAM-Signature:");

                    if (iSignaturePos >= 0)
                    {
                        // sig is in header
                        sig = hdr.Substring(iSignaturePos + "X-DSPAM-Signature:".Length);
                        sig = sig.Substring(0, sig.IndexOf(Environment.NewLine));
                        sig = sig.Trim();

                        this.logger.WriteLine("Found sig in Header: " + sig);
                    }
                }

                return sig;
            }
            catch (Exception ex)
            {
                this.logger.WriteError("getDSpamID(): " + ex.Message);
                this.logger.WriteError(ex.StackTrace);
            }

            return "";
        }
    }
}
