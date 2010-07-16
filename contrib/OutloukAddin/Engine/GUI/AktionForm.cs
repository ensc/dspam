using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using Engine.Parser;

namespace Engine.GUI
{
    public partial class AktionForm : Form
    {
        private Konfiguration config = null;
        private Logger logger = null;
        private Language language = null;
        private MailParser mailparser = null;

        public AktionForm(Konfiguration cfg, Logger log, Language lang, MailParser mp)
        {
            InitializeComponent();

            this.mailparser = mp;

            this.config = cfg;
            this.loadFromConfig();

            this.logger = log;

            this.language = lang;
            this.loadFromLanguage();

            this.tB_afterTrainSpam_MoveTo.Enabled = this.rB_afterTrainSpam_MoveTo.Checked;
            this.tB_afterTrainSpam_PrefixWith.Enabled = this.cB_afterTrainSpam_PrefixWith.Checked;
            this.tB_afterTrainHam_MoveTo.Enabled = this.rB_afterTrainHam_MoveTo.Checked;
            this.tB_afterTrainHam_PrefixWith.Enabled = this.cB_afterTrainHam_PrefixWith.Checked;
        }

        private void loadFromConfig()
        {
            Boolean bTmp;
            Boolean bTry;

            GroupBox gb = null;

            // GroupBox SPAM
            gb = (GroupBox)this.Controls["gB_afterTrainSpam"];

            // radioButtons
            if (gb.Controls[this.config.rB_afterTrainSpam] != null)
            {
                ((RadioButton)gb.Controls[this.config.rB_afterTrainSpam]).Checked = true;
            }
            else
            {
                Console.WriteLine("Control '" + this.config.rB_afterTrainSpam + "' not found.");
            }

            // checkBoxes
            bTry = Boolean.TryParse(this.config.cB_afterTrainSpam_MarkAsRead, out bTmp);
            if (bTry)
            {
                this.cB_afterTrainSpam_MarkAsRead.Checked = bTmp;
            }
            else
            {
                Console.WriteLine("cB_afterTrainSpam_MarkAsRead not Boolean: '" + this.config.cB_afterTrainSpam_MarkAsRead + "'.");
            }

            bTry = Boolean.TryParse(this.config.cB_afterTrainSpam_PrefixWith, out bTmp);
            if (bTry)
            {
                this.cB_afterTrainSpam_PrefixWith.Checked = bTmp;
            }
            else
            {
                Console.WriteLine("cB_afterTrainSpam_PrefixWith not Boolean: '" + this.config.cB_afterTrainSpam_PrefixWith + "'.");
            }

            // textBoxes
            this.tB_afterTrainSpam_MoveTo.Text = this.config.tB_afterTrainSpam_MoveTo;
            this.tB_afterTrainSpam_PrefixWith.Text = this.config.tB_afterTrainSpam_PrefixWith;

            // GroupBox HAM
            gb = (GroupBox)this.Controls["gB_afterTrainHam"];

            // radioButtons
            if (gb.Controls[this.config.rB_afterTrainHam] != null)
            {
                ((RadioButton)gb.Controls[this.config.rB_afterTrainHam]).Checked = true;
            }
            else
            {
                Console.WriteLine("Control '" + this.config.rB_afterTrainHam + "' not found.");
            }

            // checkBoxes
            bTry = Boolean.TryParse(this.config.cB_afterTrainHam_MarkAsRead, out bTmp);
            if (bTry)
            {
                this.cB_afterTrainHam_MarkAsRead.Checked = bTmp;
            }
            else
            {
                Console.WriteLine("cB_afterTrainHam_MarkAsRead not Boolean: '" + this.config.cB_afterTrainHam_MarkAsRead + "'.");
            }

            bTry = Boolean.TryParse(this.config.cB_afterTrainHam_PrefixWith, out bTmp);
            if (bTry)
            {
                this.cB_afterTrainHam_PrefixWith.Checked = bTmp;
            }
            else
            {
                Console.WriteLine("cB_afterTrainHam_PrefixWith not Boolean: '" + this.config.cB_afterTrainHam_PrefixWith + "'.");
            }

            // textBoxes
            this.tB_afterTrainHam_MoveTo.Text = this.config.tB_afterTrainHam_MoveTo;
            this.tB_afterTrainHam_PrefixWith.Text = this.config.tB_afterTrainHam_PrefixWith;
        }

        private void saveToConfig()
        {
            // GroupBox SPAM
            if (this.rB_afterTrainSpam_Nothing.Checked)
            {
                this.config.rB_afterTrainSpam = "rB_afterTrainSpam_Nothing";
            }
            else if (this.rB_afterTrainSpam_Delete.Checked)
            {
                this.config.rB_afterTrainSpam = "rB_afterTrainSpam_Delete";
            }
            else
            {
                this.config.rB_afterTrainSpam = "rB_afterTrainSpam_MoveTo";
            }

            // checkBoxes
            this.config.cB_afterTrainSpam_MarkAsRead = this.cB_afterTrainSpam_MarkAsRead.Checked.ToString();
            this.config.cB_afterTrainSpam_PrefixWith = this.cB_afterTrainSpam_PrefixWith.Checked.ToString();

            // textBoxes
            this.config.tB_afterTrainSpam_MoveTo = this.tB_afterTrainSpam_MoveTo.Text;
            this.config.tB_afterTrainSpam_PrefixWith = this.tB_afterTrainSpam_PrefixWith.Text;

            // GroupBox HAM
            // radioButtons
            if (this.rB_afterTrainHam_Nothing.Checked)
            {
                this.config.rB_afterTrainHam = "rB_afterTrainHam_Nothing";
            }
            else if (this.rB_afterTrainHam_Delete.Checked)
            {
                this.config.rB_afterTrainHam = "rB_afterTrainHam_Delete";
            }
            else
            {
                this.config.rB_afterTrainHam = "rB_afterTrainHam_MoveTo";
            }

            // checkBoxes
            this.config.cB_afterTrainHam_MarkAsRead = this.cB_afterTrainHam_MarkAsRead.Checked.ToString();
            this.config.cB_afterTrainHam_PrefixWith = this.cB_afterTrainHam_PrefixWith.Checked.ToString();

            // textBoxes
            this.config.tB_afterTrainHam_MoveTo = this.tB_afterTrainHam_MoveTo.Text;
            this.config.tB_afterTrainHam_PrefixWith = this.tB_afterTrainHam_PrefixWith.Text;
        }

        private void loadFromLanguage()
        {
            this.gB_afterTrainSpam.Text = this.language.getAktionForm("gB_afterTrainSpam");
            this.rB_afterTrainSpam_Nothing.Text = this.language.getAktionForm("rB_afterTrainSpam_Nothing");
            this.rB_afterTrainSpam_Delete.Text = this.language.getAktionForm("rB_afterTrainSpam_Delete");
            this.rB_afterTrainSpam_MoveTo.Text = this.language.getAktionForm("rB_afterTrainSpam_MoveTo");
            this.cB_afterTrainSpam_MarkAsRead.Text = this.language.getAktionForm("cB_afterTrainSpam_MarkAsRead");
            this.cB_afterTrainSpam_PrefixWith.Text = this.language.getAktionForm("cB_afterTrainSpam_PrefixWith");
            this.gB_afterTrainHam.Text = this.language.getAktionForm("gB_afterTrainHam");
            this.rB_afterTrainHam_Nothing.Text = this.language.getAktionForm("rB_afterTrainHam_Nothing");
            this.rB_afterTrainHam_Delete.Text = this.language.getAktionForm("rB_afterTrainHam_Delete");
            this.rB_afterTrainHam_MoveTo.Text = this.language.getAktionForm("rB_afterTrainHam_MoveTo");
            this.cB_afterTrainHam_MarkAsRead.Text = this.language.getAktionForm("cB_afterTrainHam_MarkAsRead");
            this.cB_afterTrainHam_PrefixWith.Text = this.language.getAktionForm("cB_afterTrainHam_PrefixWith");
            this.bt_saveForm.Text = this.language.getAktionForm("bt_saveForm");
            this.bt_closeForm.Text = this.language.getAktionForm("bt_closeForm");
        }

        private void bt_closeForm_Click(object sender, EventArgs e)
        {
            this.Close();
        }

        private void bt_saveForm_Click(object sender, EventArgs e)
        {
            this.saveToConfig();
            this.config.Save();

            this.Close();
        }

        private void bt_afterTrainSpam_MoveTo_Click(object sender, EventArgs e)
        {
            this.tB_afterTrainSpam_MoveTo.Text = this.mailparser.pickFolder();
        }

        private void bt_afterTrainHam_MoveTo_Click(object sender, EventArgs e)
        {
            this.tB_afterTrainHam_MoveTo.Text = this.mailparser.pickFolder();
        }

        private void rB_afterTrainSpam_MoveTo_CheckedChanged(object sender, EventArgs e)
        {
            this.tB_afterTrainSpam_MoveTo.Enabled = this.rB_afterTrainSpam_MoveTo.Checked;
            this.bt_afterTrainSpam_MoveTo.Enabled = this.rB_afterTrainSpam_MoveTo.Checked;
        }

        private void cB_afterTrainSpam_PrefixWith_CheckedChanged(object sender, EventArgs e)
        {
            this.tB_afterTrainSpam_PrefixWith.Enabled = this.cB_afterTrainSpam_PrefixWith.Checked;
        }

        private void rB_afterTrainHam_MoveTo_CheckedChanged(object sender, EventArgs e)
        {
            this.tB_afterTrainHam_MoveTo.Enabled = this.rB_afterTrainHam_MoveTo.Checked;
            this.bt_afterTrainHam_MoveTo.Enabled = this.rB_afterTrainHam_MoveTo.Checked;
        }

        private void cB_afterTrainHam_PrefixWith_CheckedChanged(object sender, EventArgs e)
        {
            this.tB_afterTrainHam_PrefixWith.Enabled = this.cB_afterTrainHam_PrefixWith.Checked;
        }

        private void rB_afterTrainSpam_Delete_CheckedChanged(object sender, EventArgs e)
        {
            if (this.rB_afterTrainSpam_Delete.Checked)
            {
                this.cB_afterTrainSpam_PrefixWith.Enabled = false;
                this.cB_afterTrainSpam_MarkAsRead.Enabled = false;
            }
            else
            {
                this.cB_afterTrainSpam_PrefixWith.Enabled = true;
                this.cB_afterTrainSpam_MarkAsRead.Enabled = true;
            }
        }

        private void rB_afterTrainHam_Delete_CheckedChanged(object sender, EventArgs e)
        {
            if (this.rB_afterTrainHam_Delete.Checked)
            {
                this.cB_afterTrainHam_PrefixWith.Enabled = false;
                this.cB_afterTrainHam_MarkAsRead.Enabled = false;
            }
            else
            {
                this.cB_afterTrainHam_PrefixWith.Enabled = true;
                this.cB_afterTrainHam_MarkAsRead.Enabled = true;
            }
        }
    }
}