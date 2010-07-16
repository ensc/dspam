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
    public partial class TrainingForm : Form
    {
        private Konfiguration config = null;
        private Logger logger = null;
        private Language language = null;

        private MailParser mailparser = null;

        public TrainingForm(Konfiguration cfg, Logger log, Language lang, MailParser mp)
        {
            InitializeComponent();

            this.mailparser = mp;

            this.config = cfg;
            this.loadFromConfig();

            this.logger = log;

            this.language = lang;
            this.loadFromLanguage();

            this.tB_enterFolderForHam.Enabled = this.cB_useAutoTrain.Checked;
            this.tB_enterFolderForSpam.Enabled = this.cB_useAutoTrain.Checked;
            this.tB_enterForwardForHam.Enabled = this.rB_trainWithForward.Checked;
            this.tB_enterForwardForSpam.Enabled = this.rB_trainWithForward.Checked;
            this.tB_enterLoginOfDSpam.Enabled = this.rB_trainWithWebUI.Checked;
            this.tB_enterPasswordOfDSpam.Enabled = this.rB_trainWithWebUI.Checked;
            this.tB_enterUrlOfDSpam.Enabled = this.rB_trainWithWebUI.Checked;
        }

        private void loadFromConfig()
        {
            // radioButton
            GroupBox gb = (GroupBox)this.Controls["gB_howToTrain"];
            if (gb.Controls[this.config.rB_trainWith] != null)
            {
                ((RadioButton) gb.Controls[this.config.rB_trainWith]).Checked = true;
            }
            else
            {
                Console.WriteLine("Control '" + this.config.rB_trainWith + "' not found.");
            }

            // checkBox
            Boolean bTmp;
            Boolean bTry = Boolean.TryParse(this.config.cB_useAutoTrain, out bTmp);

            if (bTry)
            {
                this.cB_useAutoTrain.Checked = bTmp;
            }
            else
            {
                Console.WriteLine("cB_useAutoTrain not Boolean: '" + this.config.cB_useAutoTrain + "'.");
            }

            // textBoxes
            this.tB_enterUrlOfDSpam.Text = this.config.tB_enterUrlOfDSpam;
            this.tB_enterLoginOfDSpam.Text = this.config.tB_enterLoginOfDSpam;
            this.tB_enterPasswordOfDSpam.Text = this.config.tB_enterPasswordOfDSpam;
            this.tB_enterForwardForSpam.Text = this.config.tB_enterForwardForSpam;
            this.tB_enterForwardForHam.Text = this.config.tB_enterForwardForHam;
            this.tB_enterFolderForSpam.Text = this.config.tB_enterFolderForSpam;
            this.tB_enterFolderForHam.Text = this.config.tB_enterFolderForHam;
        }

        private void saveToConfig()
        {
            // radioButton
            if (this.rB_trainWithWebUI.Checked)
            {
                this.config.rB_trainWith = "rB_trainWithWebUI";
            }
            else
            {
                this.config.rB_trainWith = "rB_trainWithForward";
            }

            // checkBox
            this.config.cB_useAutoTrain = this.cB_useAutoTrain.Checked.ToString();

            // textBoxes
            this.config.tB_enterUrlOfDSpam = this.tB_enterUrlOfDSpam.Text;
            this.config.tB_enterLoginOfDSpam = this.tB_enterLoginOfDSpam.Text;
            this.config.tB_enterPasswordOfDSpam = this.tB_enterPasswordOfDSpam.Text;
            this.config.tB_enterForwardForSpam = this.tB_enterForwardForSpam.Text;
            this.config.tB_enterForwardForHam = this.tB_enterForwardForHam.Text;
            this.config.tB_enterFolderForSpam = this.tB_enterFolderForSpam.Text;
            this.config.tB_enterFolderForHam = this.tB_enterFolderForHam.Text;
        }

        private void loadFromLanguage()
        {
            this.rB_trainWithWebUI.Text = this.language.getTrainingForm("rB_trainWithWebUI");
            this.l_enterUrlOfDSpam.Text = this.language.getTrainingForm("l_enterUrlOfDSpam");
            this.l_enterLoginOfDSpam.Text = this.language.getTrainingForm("l_enterLoginOfDSpam");
            this.l_enterPasswordOfDSpam.Text = this.language.getTrainingForm("l_enterPasswordOfDSpam");
            this.rB_trainWithForward.Text = this.language.getTrainingForm("rB_trainWithForward");
            this.l_enterForwardForSpam.Text = this.language.getTrainingForm("l_enterForwardForSpam");
            this.l_enterForwardForHam.Text = this.language.getTrainingForm("l_enterForwardForHam");
            this.cB_useAutoTrain.Text = this.language.getTrainingForm("cB_useAutoTrain");
            this.l_enterFolderForSpam.Text = this.language.getTrainingForm("l_enterFolderForSpam");
            this.l_enterFolderForHam.Text = this.language.getTrainingForm("l_enterFolderForHam");
            this.bt_saveForm.Text = this.language.getTrainingForm("bt_saveForm");
            this.bt_closeForm.Text = this.language.getTrainingForm("bt_closeForm");
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

        private void cB_useAutoTrain_CheckedChanged(object sender, EventArgs e)
        {
            this.tB_enterFolderForSpam.Enabled = this.cB_useAutoTrain.Checked;
            this.tB_enterFolderForHam.Enabled = this.cB_useAutoTrain.Checked;

            this.bt_enterFolderForSpam.Enabled = this.cB_useAutoTrain.Checked;
            this.bt_enterFolderForHam.Enabled = this.cB_useAutoTrain.Checked;
        }

        private void rB_trainWithForward_CheckedChanged(object sender, EventArgs e)
        {
            this.tB_enterForwardForHam.Enabled = this.rB_trainWithForward.Checked;
            this.tB_enterForwardForSpam.Enabled = this.rB_trainWithForward.Checked;
        }

        private void rB_trainWithWebUI_CheckedChanged(object sender, EventArgs e)
        {
            this.tB_enterLoginOfDSpam.Enabled = this.rB_trainWithWebUI.Checked;
            this.tB_enterPasswordOfDSpam.Enabled = this.rB_trainWithWebUI.Checked;
            this.tB_enterUrlOfDSpam.Enabled = this.rB_trainWithWebUI.Checked;
        }

        private void rB_enterFolderForSpam_Click(object sender, EventArgs e)
        {
            this.tB_enterFolderForSpam.Text = this.mailparser.pickFolder();
        }

        private void rB_enterFolderForHam_Click(object sender, EventArgs e)
        {
            this.tB_enterFolderForHam.Text = this.mailparser.pickFolder();
        }
    }
}