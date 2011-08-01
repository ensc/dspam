using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;
using Engine;
using Engine.GUI;
using Engine.Parser;

namespace simpleConsole
{
    class Program
    {
        static void Main(string[] args)
        {
            Application.EnableVisualStyles();
            Application.DoEvents();

            try
            {
                KonfigurationParser kp = new KonfigurationParser(@"c:\dspam_config.xml");
                kp.Load();
                // Console.WriteLine(kp.getFileVersion());
                /*
                Konfiguration cfg = new Konfiguration(@"dspam_config.xml");
                cfg.Load();

                return;

                Logger logger = new Logger(@"c:\temp\addin.txt");

                Language lang = new Language(cfg, logger);
                lang.Load();
                
                Worker w = new Worker(cfg, logger, lang);
                w.Run(true);
                */

                // Console.WriteLine(lang.getToolbarCaption("cB_Button_AsSpam"));

                // MainForm af = new MainForm(cfg, logger, lang, null);
                // TrainingForm af = new TrainingForm(cfg, logger, lang);
                // AktionForm af = new AktionForm(cfg, logger, lang, mp);
                // af.ShowDialog();
                
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.Message);
                Console.WriteLine(ex.StackTrace);
            }
/*
            Konfiguration cfg = new Konfiguration(@"dspam_config.xml");
            cfg.Load();

            Logger logger = new Logger(@"addin.txt");

            MainForm af = new MainForm(cfg, logger);
            af.ShowDialog();
*/
/*
            try
            {
                KonfigurationParser cp = new KonfigurationParser(@"c:\dspam_config.xml");
                cp.Load();
                // Console.WriteLine(cp.getValue("AktionForm/rB_trainWith"));
                // Console.WriteLine(cfg.rB_trainWith);
                cp.setValue("TrainingForm/cB_useAutoTrain", "True");
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.Message);
                Console.WriteLine(ex.StackTrace);
            }
 */
        }
    }
}
