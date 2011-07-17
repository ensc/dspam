using System;
using System.Collections.Generic;
using System.Text;
using Engine.Parser;

namespace Engine
{
    public class Language : IDisposable
    {
        private Konfiguration config = null;
        private Logger logger = null;
        private LanguageParser parser = null;

        public Language(Konfiguration cfg, Logger log)
        {
            this.config = cfg;
            this.logger = log;

            this.parser = new LanguageParser(this.config, this.logger);
        }

        ~Language()
        {
            this.Dispose();
        }
        
        public void Dispose()
        { }

        public void Load()
        {
            this.parser.Load();
        }

        public void Save()
        {
            throw new NotImplementedException("Save() isn't IMPLEMENTED!");
        }

        public String GetText(String which)
        {
            return this.parser.GetText(which);
        }

        public void SetText(String where, String what)
        {
            throw new NotImplementedException("SetText(where, what) isn't IMPLEMENTED!");
        }

        public String getMainForm(String what)
        {
            return this.parser.GetText("MainForm/" + what);
        }

        public String getAktionForm(String what)
        {
            return this.parser.GetText("AktionForm/" + what);
        }

        public String getTrainingForm(String what)
        {
            return this.parser.GetText("TrainingForm/" + what);
        }

        public String getUpdateBox(String what)
        {
            return this.parser.GetText("checkForUpdate/" + what);
        }

        public String getToolbarCaption(String what)
        {
            return this.parser.GetText("Toolbar/Caption/" + what);
        }

        public String getToolbarTag(String what)
        {
            return this.parser.GetText("Toolbar/Tag/" + what);
        }
    }
}
