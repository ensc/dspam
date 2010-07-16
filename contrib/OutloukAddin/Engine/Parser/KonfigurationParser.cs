using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Xml;
using System.Xml.XPath;

namespace Engine.Parser
{
    public  class KonfigurationParser : IDisposable
    {
        public static String ENTRY_NOT_FOUND = "ENTRY_NOT_FOUND";

        private String strDatei = "";

        private XmlDocument xdoc = null;
        private XPathNavigator xnav = null;

        public KonfigurationParser(String datei)
        {
            this.strDatei = datei;
        }

        public KonfigurationParser()
        {
        }

        ~KonfigurationParser()
        {
            this.Dispose();
        }

        public void Dispose()
        {
            this.xnav = null;
            this.xdoc = null;
        }

        public String XMLFile
        {
            get { return this.strDatei;  }
            set { this.strDatei = value; }
        }

        public void Load()
        {
            this.xdoc = new XmlDocument();
            this.xdoc.Load(this.strDatei);

            this.xnav = this.xdoc.CreateNavigator();
        }

        public void Save()
        {
            try
            {
                File.Copy(this.strDatei, this.strDatei.Replace("xml", "bak"), true);
            }
            catch (Exception ex)
            {
            }

            this.xdoc.Save(this.strDatei);
        }

        public String FileVersion
        {
            get { return this.xdoc.DocumentElement.Attributes["version"].Value; }
            set { this.xdoc.DocumentElement.Attributes["version"].Value = value;  }
        }

        public String getValue(String which)
        {
            try
            {
                XPathNavigator nav = this.xnav.SelectSingleNode("/addin_config/" + which);

                if (nav == null)
                {
                    return ENTRY_NOT_FOUND;
                }
                else
                {
                    return nav.Value;
                }
            }
            catch
            {
                return ENTRY_NOT_FOUND;
            }
        }


        private void setRecursiveValue(XmlNode list, String checkFor, String what )
        {
            if (checkFor == "")
            {
                return;
            }

            String current = checkFor.Substring(0, checkFor.IndexOf('/'));
            String next = checkFor.Substring(checkFor.IndexOf('/') + 1);
            
            if (list[current] == null)
            {
                if (next != "")
                {
                    XmlNode tmp = this.xdoc.CreateNode(XmlNodeType.Element, current, null);
                    list.AppendChild(tmp);
                }
                else
                {
                    XmlNode tmp = this.xdoc.CreateNode(XmlNodeType.Element, current, null);
                    tmp.InnerText = what;
                    list.AppendChild(tmp);
                }
            }

            this.setRecursiveValue(list[current], next, what);
        }

        
        public void setValue(String which, String what)
        {
            if (this.getValue(which) == ENTRY_NOT_FOUND)
            {
                // insert
                this.setRecursiveValue(this.xdoc.DocumentElement, which + "/", what);
            }
            else
            {
                // update
                XPathNavigator nav = this.xnav.SelectSingleNode("/addin_config/" + which);
                nav.SetValue(what);
            }
        }
    }
}
