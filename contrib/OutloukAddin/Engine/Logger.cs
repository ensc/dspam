using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace Engine
{
    public class Logger : IDisposable
    {
        private StreamWriter sw = null;
        private String file = "";

        public Logger(String logfile)
        {
            this.file = logfile;

            this.sw = new StreamWriter(this.file, true, Encoding.Default);
            this.sw.AutoFlush = true;
            this.sw.WriteLine(" ----- " + DateTime.Now.ToShortDateString() + " ----- ");
            this.sw.WriteLine("Version:    20070315, DSpamAddin v2");
        }

        ~Logger()
        {
            this.Dispose();
        }

        public void Dispose()
        {
            if (this.sw != null)
            {
                try
                {
                    this.sw.WriteLine("Dispose (" + DateTime.Now.ToShortDateString() + ")");
                    this.sw.Close();
                    this.sw = null;
                }
                catch (Exception ex)
                {
                }
            }
        }

        public String Cycle()
        {
            if (this.sw != null)
            {
                this.sw.Close();
                File.Copy(this.file, this.file.Replace(".", "-save."), true);
                this.sw = new StreamWriter(this.file, true, Encoding.Default);
            }

            return this.file.Replace(".", "-save.");
        }
        public void WriteLine()
        {
            if (this.sw != null)
            {
                this.sw.WriteLine();
            }
        }


        public void WriteLine(String text)
        {
            Console.WriteLine(DateTime.Now.ToShortTimeString() + "     " + text);

            if (this.sw != null)
            {
                this.sw.WriteLine(DateTime.Now.ToShortTimeString() + "     " + text);
            }
        }

        public void WriteError(String text)
        {
            Console.WriteLine(DateTime.Now.ToShortTimeString() + " ### " + text);

            if (this.sw != null)
            {
                this.sw.WriteLine(DateTime.Now.ToShortTimeString() + " ### " + text);
            }
        }
    }
}
