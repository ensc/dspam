using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Resources;
using System.Security.Cryptography;
using System.Text;
using System.Xml;

namespace Engine.Parser
{
    // converts the user supplied xml language files to
    // usable code language files
    public class LanguageParser : IDisposable
    {
        private Konfiguration config = null;
        private Logger logger = null;

        private String _lang_home = "";
        private ResourceManager _rm = null;

        public LanguageParser(Konfiguration cfg, Logger log)
        {
            this.config = cfg;
            this.logger = log;

            String temp = Assembly.GetCallingAssembly().Location;
            temp = temp.Substring(0, temp.LastIndexOf(@"\"));
            this._lang_home = temp + @"\lang\";

            // this._lang_home = @"c:\_lang\";
            this.logger.WriteLine("Language Directory is " + this._lang_home);
        }

        ~LanguageParser()
        {
            this.Dispose();
        }

        public void Dispose()
        {
        }

        public String GetText(String which)
        {
            return this._rm.GetString(which);
        }

        public void Load()
        {
            // check if language dir exists
            if (Directory.Exists(this._lang_home) == false)
            {
                Directory.CreateDirectory(this._lang_home);
            }

            if (File.Exists(this._lang_home + @"default.xml") == false)
            {
                this.createDefaultLanguage();
            }

            this.checkLanguages();

            this._rm = ResourceManager.CreateFileBasedResourceManager("Languages", this._lang_home, null);
        }

        private void createDefaultLanguage()
        {
            Assembly a = Assembly.GetExecutingAssembly();

            Stream fs = a.GetManifestResourceStream("Engine.en-US.xml");
            StreamReader sr = new StreamReader(fs);
            StreamWriter sw = new StreamWriter(this._lang_home + @"default.xml", false, Encoding.Default);
            sw.WriteLine(sr.ReadToEnd());
            sw.Close();
            sr.Close();
            fs.Close();

            XmlDocument xdoc = new XmlDocument();
            xdoc.Load(this._lang_home + @"default.xml");
            ResourceWriter rw = new ResourceWriter(this._lang_home + "Languages.resources");

            foreach (XmlNode n in xdoc.ChildNodes[1])
            {
                this.generateRessource(rw, n, "");
            }

            rw.Generate();
            rw.Close();
        }

        private void checkLanguages()
        {
            // check hashes/size of xml-files. if differ, recreate .resource
            DirectoryInfo di = new DirectoryInfo(this._lang_home);
            foreach (FileInfo fi in di.GetFiles("??-??.xml"))
            {
                String lang = fi.Name.Replace(fi.Extension, "");
                if (File.Exists(this._lang_home + "Languages." + lang + ".resources") && (this.md5(fi.DirectoryName + @"\" + fi.Name) == this.config.getLanguageHash(lang)))
                {
                    // its already converted. and haven't changed since.
                }
                else
                {
                    try
                    {
                        // new language
                        XmlDocument xdoc = new XmlDocument();
                        xdoc.Load(this._lang_home + fi.Name);

                        ResourceWriter rw = new ResourceWriter(this._lang_home + "Languages." + lang + ".resources");

                        foreach (XmlNode n in xdoc.ChildNodes[1])
                        {
                            this.generateRessource(rw, n, "");
                        }

                        rw.Generate();
                        rw.Close();

                        this.config.setLanguageHash(lang, this.md5(fi.DirectoryName + @"\" + fi.Name));
                    }
                    catch (Exception ex)
                    {
                        // ooops.
                        this.logger.WriteError("checkLanguages(" + lang + "): " + ex.Message);
                        this.logger.WriteError(ex.StackTrace);
                    }
                }
            }
        }

        private String md5(String file)
        {
            try
            {
                if (File.Exists(file) == false)
                {
                    this.logger.WriteLine("md5() says file not found: '" + file + "'");
                    return "";
                }

                FileStream file1 = new FileStream(file, FileMode.Open, FileAccess.Read);
                MD5CryptoServiceProvider md5 = new MD5CryptoServiceProvider();
                md5.ComputeHash(file1);

                byte[] hash = md5.Hash;
                StringBuilder buff = new StringBuilder();

                foreach (byte hashByte in hash)
                {
                    buff.Append(String.Format("{0:X1}", hashByte));
                }

                file1.Close();

                return buff.ToString();
            }
            catch( Exception ex)
            {
                this.logger.WriteError("md5(): " + ex.Message);
                this.logger.WriteError(ex.StackTrace);

                return "";
            }
        }

        private void generateRessource(ResourceWriter rw, XmlNode node, String pfad)
        {
            if (node.HasChildNodes)
            {
                pfad += "/" + node.Name;

                foreach (XmlNode o in node)
                {
                    this.generateRessource(rw, o, pfad);
                }
            }
            else
            {
                this.logger.WriteLine("gR: " + pfad.Substring(1) + " => " + node.InnerText);
                rw.AddResource(pfad.Substring(1), node.InnerText);
            }
        }
    }
}
