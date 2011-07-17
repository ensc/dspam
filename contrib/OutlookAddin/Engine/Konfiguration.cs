using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Text;
using Engine.Parser;

namespace Engine
{
    // Event "SettingsGotChanged"
    public delegate void SettingsGotChanged();

    public class Konfiguration : IDisposable
    {
        public event SettingsGotChanged SettingsChanged;

        private readonly KonfigurationParser parser = null;
        private readonly Logger logger = null;

        // hardcoded stuff
        private readonly String strVersion = "2.0.3";

        private readonly String strAdvertisingText = "Hello everybody," + Environment.NewLine + "I hope you like my little Outlook-AddIn." + Environment.NewLine + "It's brought to you by MAS-User-Services (http://www.mas-user-services.de).";
        private readonly String strThankYouMailAddress = "isopropylbenzol@gmail.com";

        private readonly String _url_spam = "dspam.cgi?template=historyIN&history_page=1&user=%USER%&retrain=spam&signatureID=";
        private readonly String _url_ham = "dspam.cgi?template=historyIN&history_page=1&user=%USER%&retrain=innocent&signatureID=";
/*
        private readonly String _url_spam = "dspam.cgi?template=history&history_page=1&user=%USER%&retrain=spam&signatureID=";
        private readonly String _url_ham = "dspam.cgi?template=history&history_page=1&user=%USER%&retrain=innocent&signatureID=";
*/
        private readonly String _home_dir = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData) + @"\dspam\";
        private readonly String _default_dir = Assembly.GetCallingAssembly().Location.Substring(0, Assembly.GetCallingAssembly().Location.LastIndexOf(@"\")) + @"\";
        private String _file = "";

        private void onSettingsChanged()
        {
            if (this.SettingsChanged != null)
            {
                this.SettingsChanged();
            }
        }

        public Konfiguration(String filename, Logger log)
        {
            this.logger = log;
            this.logger.WriteLine("loading configuration...");

            // if $home/dspam/dspam-config.xml exists, load it
            // else load from $install_dir/dspam-config.xml
            this._file = filename;

            if (Directory.Exists(this._home_dir) == false)
            {
                Directory.CreateDirectory(this._home_dir);
            }

            if (File.Exists(this._home_dir + filename))
            {
                this.logger.WriteLine("using XML from $HOME");
                this.parser = new KonfigurationParser(this._home_dir + filename);
            }
            else
            {
                this.logger.WriteLine("using XML from DEFAULT_DIR");
                this.parser = new KonfigurationParser(this._default_dir + filename);
            }
        }

        ~Konfiguration()
        {
            this.Dispose();
        }

        public void Dispose()
        {}

        public void Load()
        {
            this.parser.Load();
            this.CheckVersion();
            this.showConfiguration();
        }

        public void Save()
        {
            this.parser.XMLFile = this._home_dir + this._file;
            this.parser.Save();

            this.onSettingsChanged();
        }

        public String Version
        {
            get { return this.strVersion; }
        }

        private void CheckVersion()
        {
            if (int.Parse(this.Version.Replace(".", "")) > int.Parse(this.parser.FileVersion.Replace(".", "")))
            {
                // old config found!
                this.logger.WriteLine("version of configuration is older then the installed version");
                this.parser.FileVersion = this.Version;
            }
        }

        private void showConfiguration()
        {
            this.logger.WriteLine("actual configuration:");
            if (this.DoTrainWithHistory)
            {
                this.logger.WriteLine("* using WebUI for training.");
                this.logger.WriteLine("  " + this.tB_enterUrlOfDSpam);
            }
            else
            {
                this.logger.WriteLine("* using Mailforward for training.");
                this.logger.WriteLine("  SPAM: " + this.ForwardForSpam);
                this.logger.WriteLine("   HAM: " + this.ForwardForHam);
            }

            this.logger.WriteLine("SPAM");
            if (this.AfterTraining_Spam_DoDelete)
            {
                this.logger.WriteLine("* delete after training");
            }
            if (this.AfterTraining_Spam_MarkAsRead)
            {
                this.logger.WriteLine("* mark read after training");
            }
            if (this.AfterTraining_Spam_DoPrefix)
            {
                this.logger.WriteLine("* prefix with '" + this.AfterTraining_Spam_PrefixWith + "' after training");
            }
            if (this.AfterTraining_Spam_DoMove)
            {
                this.logger.WriteLine("* move to folder '" + this.AfterTraining_Spam_MoveTo + "' after training");
            }

            this.logger.WriteLine("HAM");
            if (this.AfterTraining_Ham_DoDelete)
            {
                this.logger.WriteLine("* delete after training");
            }
            if (this.AfterTraining_Ham_MarkAsRead)
            {
                this.logger.WriteLine("* mark read after training");
            }
            if (this.AfterTraining_Ham_DoPrefix)
            {
                this.logger.WriteLine("* remove '" + this.AfterTraining_Ham_PrefixWith + "' from mail after training");
            }
            if (this.AfterTraining_Ham_DoMove)
            {
                this.logger.WriteLine("* move to folder '" + this.AfterTraining_Ham_MoveTo + "' after training");
            }
            
            if (this.UseAutoTrain)
            {
                this.logger.WriteLine("AutoTrain: ENABLED");
                this.logger.WriteLine("Trainingfolder for SPAM: " + this.AutoTraining_Spam);
                this.logger.WriteLine("Trainingfolder for HAM:  " + this.AutoTraining_Ham);
            }
            else
            {
                this.logger.WriteLine("AutoTrain: DISABLED");
            }
        }

        #region MainForm
        /*
            Contains:
            String: Advertising Text for tb_advertising (HARDCODED)
            String: E-Mail-Address where the ThankYou-Mail goes to (HARDCODED)
            String: E-Mail-Address where the Log goes to (from XML)
            String: URL to check for Update (from XML)
        */
        public String Text_Advertising
        {
            get { return this.strAdvertisingText; }
        }

        public String Address_ThankYou
        {
            get { return this.strThankYouMailAddress; }
        }

        public String Address_LogGoesTo
        {
            get { return this.parser.getValue("Address_LogGoesTo"); }
            set { this.parser.setValue("Address_LogGoesTo", value); }
        }

        public String Url_CheckForUpdate
        {
            get { return this.parser.getValue("Url_CheckForUpdate"); }
            set { this.parser.setValue("Url_CheckForUpdate", value); }
        }
        #endregion

        #region AktionForm
        #region SPAM
        public String rB_afterTrainSpam
        {
            get { return this.parser.getValue("AktionForm/rB_afterTrainSpam"); }
            set { this.parser.setValue("AktionForm/rB_afterTrainSpam", value); }
        }

        public String tB_afterTrainSpam_MoveTo
        {
            get { return this.parser.getValue("AktionForm/tB_afterTrainSpam_MoveTo"); }
            set { this.parser.setValue("AktionForm/tB_afterTrainSpam_MoveTo", value); }
        }

        public String cB_afterTrainSpam_MarkAsRead
        {
            get { return this.parser.getValue("AktionForm/cB_afterTrainSpam_MarkAsRead"); }
            set { this.parser.setValue("AktionForm/cB_afterTrainSpam_MarkAsRead", value); }
        }

        public String cB_afterTrainSpam_PrefixWith
        {
            get { return this.parser.getValue("AktionForm/cB_afterTrainSpam_PrefixWith"); }
            set { this.parser.setValue("AktionForm/cB_afterTrainSpam_PrefixWith", value); }
        }

        public String tB_afterTrainSpam_PrefixWith
        {
            get { return this.parser.getValue("AktionForm/tB_afterTrainSpam_PrefixWith"); }
            set { this.parser.setValue("AktionForm/tB_afterTrainSpam_PrefixWith", value); }
        }
        #endregion

        #region HAM
        public String rB_afterTrainHam
        {
            get { return this.parser.getValue("AktionForm/rB_afterTrainHam"); }
            set { this.parser.setValue("AktionForm/rB_afterTrainHam", value); }
        }

        public String tB_afterTrainHam_MoveTo
        {
            get { return this.parser.getValue("AktionForm/tB_afterTrainHam_MoveTo"); }
            set { this.parser.setValue("AktionForm/tB_afterTrainHam_MoveTo", value); }
        }

        public String cB_afterTrainHam_MarkAsRead
        {
            get { return this.parser.getValue("AktionForm/cB_afterTrainHam_MarkAsRead"); }
            set { this.parser.setValue("AktionForm/cB_afterTrainHam_MarkAsRead", value); }
        }

        public String cB_afterTrainHam_PrefixWith
        {
            get { return this.parser.getValue("AktionForm/cB_afterTrainHam_PrefixWith"); }
            set { this.parser.setValue("AktionForm/cB_afterTrainHam_PrefixWith", value); }
        }

        public String tB_afterTrainHam_PrefixWith
        {
            get { return this.parser.getValue("AktionForm/tB_afterTrainHam_PrefixWith"); }
            set { this.parser.setValue("AktionForm/tB_afterTrainHam_PrefixWith", value); }
        }
        #endregion
        #endregion

        #region TrainingForm
        public String rB_trainWith
        {
            get { return this.parser.getValue("TrainingForm/rB_trainWith"); }
            set { this.parser.setValue("TrainingForm/rB_trainWith", value); }
        }

        public String cB_useAutoTrain
        {
            get { return this.parser.getValue("TrainingForm/cB_useAutoTrain"); }
            set
            {
                this.parser.setValue("TrainingForm/cB_useAutoTrain", value);
//                this.onSettingsChanged();
            }
        }

        public String tB_enterUrlOfDSpam
        {
            get { return this.parser.getValue("TrainingForm/tB_enterUrlOfDSpam"); }
            set
            {
                if (value.EndsWith("/") == false)
                {
                    value += "/";
                }

                this.parser.setValue("TrainingForm/tB_enterUrlOfDSpam", value);
            }
        }

        public String tB_enterLoginOfDSpam
        {
            get { return this.parser.getValue("TrainingForm/tB_enterLoginOfDSpam"); }
            set { this.parser.setValue("TrainingForm/tB_enterLoginOfDSpam", value); }
        }

        public String tB_enterPasswordOfDSpam
        {
            get { return this.parser.getValue("TrainingForm/tB_enterPasswordOfDSpam"); }
            set { this.parser.setValue("TrainingForm/tB_enterPasswordOfDSpam", value); }
        }

        public String tB_enterForwardForSpam
        {
            get { return this.parser.getValue("TrainingForm/tB_enterForwardForSpam"); }
            set { this.parser.setValue("TrainingForm/tB_enterForwardForSpam", value); }
        }

        public String tB_enterForwardForHam
        {
            get { return this.parser.getValue("TrainingForm/tB_enterForwardForHam"); }
            set { this.parser.setValue("TrainingForm/tB_enterForwardForHam", value); }
        }

        public String tB_enterFolderForSpam
        {
            get { return this.parser.getValue("TrainingForm/tB_enterFolderForSpam"); }
            set
            {
                this.parser.setValue("TrainingForm/tB_enterFolderForSpam", value);
//                this.onSettingsChanged();
            }
        }

        public String tB_enterFolderForHam
        {
            get { return this.parser.getValue("TrainingForm/tB_enterFolderForHam"); }
            set
            {
                this.parser.setValue("TrainingForm/tB_enterFolderForHam", value);
//                this.onSettingsChanged();
            }
        }
        #endregion

        #region Language
        public String getLanguageHash(String lang)
        {
            return this.parser.getValue("Language/" + lang);
        }

        public void setLanguageHash(String lang, String hash)
        {
            this.parser.setValue("Language/" + lang, hash);
        }
        #endregion

        #region Worker
        public Boolean DoTrainWithHistory
        {
            get
            {
                if (this.rB_trainWith == "rB_trainWithWebUI")
                    return true;
                else
                    return false;
            }
        }

        public String UrlForSpam
        {
            get
            {
                return this.tB_enterUrlOfDSpam + this._url_spam.Replace("%USER%", this.tB_enterLoginOfDSpam);
            }
        }

        public String UrlForHam
        {
            get
            {
                return this.tB_enterUrlOfDSpam + this._url_ham.Replace("%USER%", this.tB_enterLoginOfDSpam);
            }
        }

        public String DSpamLogin
        {
            get { return this.tB_enterLoginOfDSpam; }
        }

        public String DSpamPassword
        {
            get { return this.tB_enterPasswordOfDSpam; }
        }

        public String ForwardForSpam
        {
            get { return this.tB_enterForwardForSpam; }
        }

        public String ForwardForHam
        {
            get { return this.tB_enterForwardForHam; }
        }

        public Boolean AfterTraining_Spam_DoDelete
        {
            get
            {
                if (this.rB_afterTrainSpam == "rB_afterTrainSpam_Delete")
                    return true;
                else
                    return false;
            }
        }

        public Boolean AfterTraining_Spam_DoMove
        {
            get
            {
                if (this.rB_afterTrainSpam == "rB_afterTrainSpam_MoveTo")
                    return true;
                else
                    return false;
            }
        }

        public Boolean AfterTraining_Spam_MarkAsRead
        {
            get
            {
                if (this.cB_afterTrainSpam_MarkAsRead == "True")
                    return true;
                else
                    return false;
            }
        }

        public Boolean AfterTraining_Spam_DoPrefix
        {
            get
            {
                if (this.cB_afterTrainSpam_PrefixWith == "True")
                    return true;
                else
                    return false;
            }
        }

        public String AfterTraining_Spam_PrefixWith
        {
            get
            {
                return this.tB_afterTrainSpam_PrefixWith;
            }
        }

        public String AfterTraining_Spam_MoveTo
        {
            get
            {
                return this.tB_afterTrainSpam_MoveTo;
            }
        }
        
        public Boolean AfterTraining_Ham_DoDelete
        {
            get
            {
                if (this.rB_afterTrainHam == "rB_afterTrainHam_Delete")
                    return true;
                else
                    return false;
            }
        }

        public Boolean AfterTraining_Ham_DoMove
        {
            get
            {
                if (this.rB_afterTrainHam == "rB_afterTrainHam_MoveTo")
                    return true;
                else
                    return false;
            }
        }

        public Boolean AfterTraining_Ham_MarkAsRead
        {
            get
            {
                if (this.cB_afterTrainHam_MarkAsRead == "True")
                    return true;
                else
                    return false;
            }
        }

        public Boolean AfterTraining_Ham_DoPrefix
        {
            get
            {
                if (this.cB_afterTrainHam_PrefixWith == "True")
                    return true;
                else
                    return false;
            }
        }

        public String AfterTraining_Ham_PrefixWith
        {
            get
            {
                return this.tB_afterTrainHam_PrefixWith;
            }
        }

        public String AfterTraining_Ham_MoveTo
        {
            get
            {
                return this.tB_afterTrainHam_MoveTo;
            }
        }

        public String AutoTraining_Spam
        {
            get { return this.tB_enterFolderForSpam; }
        }

        public String AutoTraining_Ham
        {
            get { return this.tB_enterFolderForHam; }
        }

        public Boolean UseAutoTrain
        {
            get
            {
                if (this.cB_useAutoTrain == "True")
                    return true;
                else
                    return false;
            }
        }
        #endregion
    }
}
