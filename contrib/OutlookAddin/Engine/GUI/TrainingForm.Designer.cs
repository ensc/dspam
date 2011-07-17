namespace Engine.GUI
{
    partial class TrainingForm
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.gB_howToTrain = new System.Windows.Forms.GroupBox();
            this.bt_enterFolderForHam = new System.Windows.Forms.Button();
            this.bt_enterFolderForSpam = new System.Windows.Forms.Button();
            this.l_enterFolderForHam = new System.Windows.Forms.Label();
            this.tB_enterFolderForHam = new System.Windows.Forms.TextBox();
            this.l_enterFolderForSpam = new System.Windows.Forms.Label();
            this.tB_enterFolderForSpam = new System.Windows.Forms.TextBox();
            this.l_enterForwardForHam = new System.Windows.Forms.Label();
            this.tB_enterForwardForHam = new System.Windows.Forms.TextBox();
            this.l_enterForwardForSpam = new System.Windows.Forms.Label();
            this.tB_enterForwardForSpam = new System.Windows.Forms.TextBox();
            this.l_enterPasswordOfDSpam = new System.Windows.Forms.Label();
            this.tB_enterPasswordOfDSpam = new System.Windows.Forms.TextBox();
            this.l_enterLoginOfDSpam = new System.Windows.Forms.Label();
            this.tB_enterLoginOfDSpam = new System.Windows.Forms.TextBox();
            this.l_enterUrlOfDSpam = new System.Windows.Forms.Label();
            this.tB_enterUrlOfDSpam = new System.Windows.Forms.TextBox();
            this.cB_useAutoTrain = new System.Windows.Forms.CheckBox();
            this.rB_trainWithForward = new System.Windows.Forms.RadioButton();
            this.rB_trainWithWebUI = new System.Windows.Forms.RadioButton();
            this.bt_saveForm = new System.Windows.Forms.Button();
            this.bt_closeForm = new System.Windows.Forms.Button();
            this.gB_howToTrain.SuspendLayout();
            this.SuspendLayout();
            // 
            // gB_howToTrain
            // 
            this.gB_howToTrain.Controls.Add(this.bt_enterFolderForHam);
            this.gB_howToTrain.Controls.Add(this.bt_enterFolderForSpam);
            this.gB_howToTrain.Controls.Add(this.l_enterFolderForHam);
            this.gB_howToTrain.Controls.Add(this.tB_enterFolderForHam);
            this.gB_howToTrain.Controls.Add(this.l_enterFolderForSpam);
            this.gB_howToTrain.Controls.Add(this.tB_enterFolderForSpam);
            this.gB_howToTrain.Controls.Add(this.l_enterForwardForHam);
            this.gB_howToTrain.Controls.Add(this.tB_enterForwardForHam);
            this.gB_howToTrain.Controls.Add(this.l_enterForwardForSpam);
            this.gB_howToTrain.Controls.Add(this.tB_enterForwardForSpam);
            this.gB_howToTrain.Controls.Add(this.l_enterPasswordOfDSpam);
            this.gB_howToTrain.Controls.Add(this.tB_enterPasswordOfDSpam);
            this.gB_howToTrain.Controls.Add(this.l_enterLoginOfDSpam);
            this.gB_howToTrain.Controls.Add(this.tB_enterLoginOfDSpam);
            this.gB_howToTrain.Controls.Add(this.l_enterUrlOfDSpam);
            this.gB_howToTrain.Controls.Add(this.tB_enterUrlOfDSpam);
            this.gB_howToTrain.Controls.Add(this.cB_useAutoTrain);
            this.gB_howToTrain.Controls.Add(this.rB_trainWithForward);
            this.gB_howToTrain.Controls.Add(this.rB_trainWithWebUI);
            this.gB_howToTrain.Location = new System.Drawing.Point(12, 12);
            this.gB_howToTrain.Name = "gB_howToTrain";
            this.gB_howToTrain.Size = new System.Drawing.Size(692, 327);
            this.gB_howToTrain.TabIndex = 0;
            this.gB_howToTrain.TabStop = false;
            // 
            // bt_enterFolderForHam
            // 
            this.bt_enterFolderForHam.Image = global::Engine.Properties.Resources.folder;
            this.bt_enterFolderForHam.Location = new System.Drawing.Point(636, 290);
            this.bt_enterFolderForHam.Name = "bt_enterFolderForHam";
            this.bt_enterFolderForHam.Size = new System.Drawing.Size(27, 23);
            this.bt_enterFolderForHam.TabIndex = 18;
            this.bt_enterFolderForHam.UseVisualStyleBackColor = true;
            this.bt_enterFolderForHam.Click += new System.EventHandler(this.rB_enterFolderForHam_Click);
            // 
            // bt_enterFolderForSpam
            // 
            this.bt_enterFolderForSpam.Image = global::Engine.Properties.Resources.folder;
            this.bt_enterFolderForSpam.Location = new System.Drawing.Point(636, 261);
            this.bt_enterFolderForSpam.Name = "bt_enterFolderForSpam";
            this.bt_enterFolderForSpam.Size = new System.Drawing.Size(27, 23);
            this.bt_enterFolderForSpam.TabIndex = 17;
            this.bt_enterFolderForSpam.UseVisualStyleBackColor = true;
            this.bt_enterFolderForSpam.Click += new System.EventHandler(this.rB_enterFolderForSpam_Click);
            // 
            // l_enterFolderForHam
            // 
            this.l_enterFolderForHam.AutoSize = true;
            this.l_enterFolderForHam.Location = new System.Drawing.Point(25, 292);
            this.l_enterFolderForHam.Name = "l_enterFolderForHam";
            this.l_enterFolderForHam.Size = new System.Drawing.Size(105, 13);
            this.l_enterFolderForHam.TabIndex = 16;
            this.l_enterFolderForHam.Text = "l_enterFolderForHam";
            // 
            // tB_enterFolderForHam
            // 
            this.tB_enterFolderForHam.Location = new System.Drawing.Point(277, 289);
            this.tB_enterFolderForHam.Name = "tB_enterFolderForHam";
            this.tB_enterFolderForHam.Size = new System.Drawing.Size(353, 20);
            this.tB_enterFolderForHam.TabIndex = 15;
            // 
            // l_enterFolderForSpam
            // 
            this.l_enterFolderForSpam.AutoSize = true;
            this.l_enterFolderForSpam.Location = new System.Drawing.Point(25, 266);
            this.l_enterFolderForSpam.Name = "l_enterFolderForSpam";
            this.l_enterFolderForSpam.Size = new System.Drawing.Size(110, 13);
            this.l_enterFolderForSpam.TabIndex = 14;
            this.l_enterFolderForSpam.Text = "l_enterFolderForSpam";
            // 
            // tB_enterFolderForSpam
            // 
            this.tB_enterFolderForSpam.Location = new System.Drawing.Point(277, 263);
            this.tB_enterFolderForSpam.Name = "tB_enterFolderForSpam";
            this.tB_enterFolderForSpam.Size = new System.Drawing.Size(353, 20);
            this.tB_enterFolderForSpam.TabIndex = 13;
            // 
            // l_enterForwardForHam
            // 
            this.l_enterForwardForHam.AutoSize = true;
            this.l_enterForwardForHam.Location = new System.Drawing.Point(25, 199);
            this.l_enterForwardForHam.Name = "l_enterForwardForHam";
            this.l_enterForwardForHam.Size = new System.Drawing.Size(114, 13);
            this.l_enterForwardForHam.TabIndex = 12;
            this.l_enterForwardForHam.Text = "l_enterForwardForHam";
            // 
            // tB_enterForwardForHam
            // 
            this.tB_enterForwardForHam.Location = new System.Drawing.Point(204, 196);
            this.tB_enterForwardForHam.Name = "tB_enterForwardForHam";
            this.tB_enterForwardForHam.Size = new System.Drawing.Size(426, 20);
            this.tB_enterForwardForHam.TabIndex = 11;
            // 
            // l_enterForwardForSpam
            // 
            this.l_enterForwardForSpam.AutoSize = true;
            this.l_enterForwardForSpam.Location = new System.Drawing.Point(25, 173);
            this.l_enterForwardForSpam.Name = "l_enterForwardForSpam";
            this.l_enterForwardForSpam.Size = new System.Drawing.Size(119, 13);
            this.l_enterForwardForSpam.TabIndex = 10;
            this.l_enterForwardForSpam.Text = "l_enterForwardForSpam";
            // 
            // tB_enterForwardForSpam
            // 
            this.tB_enterForwardForSpam.Location = new System.Drawing.Point(204, 170);
            this.tB_enterForwardForSpam.Name = "tB_enterForwardForSpam";
            this.tB_enterForwardForSpam.Size = new System.Drawing.Size(426, 20);
            this.tB_enterForwardForSpam.TabIndex = 9;
            // 
            // l_enterPasswordOfDSpam
            // 
            this.l_enterPasswordOfDSpam.AutoSize = true;
            this.l_enterPasswordOfDSpam.Location = new System.Drawing.Point(25, 96);
            this.l_enterPasswordOfDSpam.Name = "l_enterPasswordOfDSpam";
            this.l_enterPasswordOfDSpam.Size = new System.Drawing.Size(131, 13);
            this.l_enterPasswordOfDSpam.TabIndex = 8;
            this.l_enterPasswordOfDSpam.Text = "l_enterPasswordOfDSpam";
            // 
            // tB_enterPasswordOfDSpam
            // 
            this.tB_enterPasswordOfDSpam.Location = new System.Drawing.Point(204, 93);
            this.tB_enterPasswordOfDSpam.Name = "tB_enterPasswordOfDSpam";
            this.tB_enterPasswordOfDSpam.Size = new System.Drawing.Size(426, 20);
            this.tB_enterPasswordOfDSpam.TabIndex = 7;
            // 
            // l_enterLoginOfDSpam
            // 
            this.l_enterLoginOfDSpam.AutoSize = true;
            this.l_enterLoginOfDSpam.Location = new System.Drawing.Point(25, 70);
            this.l_enterLoginOfDSpam.Name = "l_enterLoginOfDSpam";
            this.l_enterLoginOfDSpam.Size = new System.Drawing.Size(111, 13);
            this.l_enterLoginOfDSpam.TabIndex = 6;
            this.l_enterLoginOfDSpam.Text = "l_enterLoginOfDSpam";
            // 
            // tB_enterLoginOfDSpam
            // 
            this.tB_enterLoginOfDSpam.Location = new System.Drawing.Point(204, 67);
            this.tB_enterLoginOfDSpam.Name = "tB_enterLoginOfDSpam";
            this.tB_enterLoginOfDSpam.Size = new System.Drawing.Size(426, 20);
            this.tB_enterLoginOfDSpam.TabIndex = 5;
            // 
            // l_enterUrlOfDSpam
            // 
            this.l_enterUrlOfDSpam.AutoSize = true;
            this.l_enterUrlOfDSpam.Location = new System.Drawing.Point(25, 44);
            this.l_enterUrlOfDSpam.Name = "l_enterUrlOfDSpam";
            this.l_enterUrlOfDSpam.Size = new System.Drawing.Size(98, 13);
            this.l_enterUrlOfDSpam.TabIndex = 4;
            this.l_enterUrlOfDSpam.Text = "l_enterUrlOfDSpam";
            // 
            // tB_enterUrlOfDSpam
            // 
            this.tB_enterUrlOfDSpam.Location = new System.Drawing.Point(204, 41);
            this.tB_enterUrlOfDSpam.Name = "tB_enterUrlOfDSpam";
            this.tB_enterUrlOfDSpam.Size = new System.Drawing.Size(426, 20);
            this.tB_enterUrlOfDSpam.TabIndex = 3;
            // 
            // cB_useAutoTrain
            // 
            this.cB_useAutoTrain.AutoSize = true;
            this.cB_useAutoTrain.Location = new System.Drawing.Point(6, 234);
            this.cB_useAutoTrain.Name = "cB_useAutoTrain";
            this.cB_useAutoTrain.Size = new System.Drawing.Size(108, 17);
            this.cB_useAutoTrain.TabIndex = 2;
            this.cB_useAutoTrain.Text = "cB_useAutoTrain";
            this.cB_useAutoTrain.UseVisualStyleBackColor = true;
            this.cB_useAutoTrain.CheckedChanged += new System.EventHandler(this.cB_useAutoTrain_CheckedChanged);
            // 
            // rB_trainWithForward
            // 
            this.rB_trainWithForward.AutoSize = true;
            this.rB_trainWithForward.Location = new System.Drawing.Point(6, 139);
            this.rB_trainWithForward.Name = "rB_trainWithForward";
            this.rB_trainWithForward.Size = new System.Drawing.Size(121, 17);
            this.rB_trainWithForward.TabIndex = 1;
            this.rB_trainWithForward.Text = "rB_trainWithForward";
            this.rB_trainWithForward.UseVisualStyleBackColor = true;
            this.rB_trainWithForward.CheckedChanged += new System.EventHandler(this.rB_trainWithForward_CheckedChanged);
            // 
            // rB_trainWithWebUI
            // 
            this.rB_trainWithWebUI.AutoSize = true;
            this.rB_trainWithWebUI.Checked = true;
            this.rB_trainWithWebUI.Location = new System.Drawing.Point(6, 19);
            this.rB_trainWithWebUI.Name = "rB_trainWithWebUI";
            this.rB_trainWithWebUI.Size = new System.Drawing.Size(117, 17);
            this.rB_trainWithWebUI.TabIndex = 0;
            this.rB_trainWithWebUI.TabStop = true;
            this.rB_trainWithWebUI.Text = "rB_trainWithWebUI";
            this.rB_trainWithWebUI.UseVisualStyleBackColor = true;
            this.rB_trainWithWebUI.CheckedChanged += new System.EventHandler(this.rB_trainWithWebUI_CheckedChanged);
            // 
            // bt_saveForm
            // 
            this.bt_saveForm.Location = new System.Drawing.Point(12, 345);
            this.bt_saveForm.Name = "bt_saveForm";
            this.bt_saveForm.Size = new System.Drawing.Size(75, 23);
            this.bt_saveForm.TabIndex = 1;
            this.bt_saveForm.Text = "bt_saveForm";
            this.bt_saveForm.UseVisualStyleBackColor = true;
            this.bt_saveForm.Click += new System.EventHandler(this.bt_saveForm_Click);
            // 
            // bt_closeForm
            // 
            this.bt_closeForm.Location = new System.Drawing.Point(576, 345);
            this.bt_closeForm.Name = "bt_closeForm";
            this.bt_closeForm.Size = new System.Drawing.Size(75, 23);
            this.bt_closeForm.TabIndex = 5;
            this.bt_closeForm.Text = "bt_closeForm";
            this.bt_closeForm.UseVisualStyleBackColor = true;
            this.bt_closeForm.Click += new System.EventHandler(this.bt_closeForm_Click);
            // 
            // TrainingForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(729, 386);
            this.Controls.Add(this.bt_closeForm);
            this.Controls.Add(this.bt_saveForm);
            this.Controls.Add(this.gB_howToTrain);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedToolWindow;
            this.Name = "TrainingForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "TrainingForm";
            this.gB_howToTrain.ResumeLayout(false);
            this.gB_howToTrain.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.GroupBox gB_howToTrain;
        private System.Windows.Forms.RadioButton rB_trainWithForward;
        private System.Windows.Forms.RadioButton rB_trainWithWebUI;
        private System.Windows.Forms.CheckBox cB_useAutoTrain;
        private System.Windows.Forms.Label l_enterPasswordOfDSpam;
        private System.Windows.Forms.TextBox tB_enterPasswordOfDSpam;
        private System.Windows.Forms.Label l_enterLoginOfDSpam;
        private System.Windows.Forms.TextBox tB_enterLoginOfDSpam;
        private System.Windows.Forms.Label l_enterUrlOfDSpam;
        private System.Windows.Forms.TextBox tB_enterUrlOfDSpam;
        private System.Windows.Forms.Label l_enterForwardForHam;
        private System.Windows.Forms.TextBox tB_enterForwardForHam;
        private System.Windows.Forms.Label l_enterForwardForSpam;
        private System.Windows.Forms.TextBox tB_enterForwardForSpam;
        private System.Windows.Forms.Label l_enterFolderForHam;
        private System.Windows.Forms.TextBox tB_enterFolderForHam;
        private System.Windows.Forms.Label l_enterFolderForSpam;
        private System.Windows.Forms.TextBox tB_enterFolderForSpam;
        private System.Windows.Forms.Button bt_saveForm;
        private System.Windows.Forms.Button bt_closeForm;
        private System.Windows.Forms.Button bt_enterFolderForHam;
        private System.Windows.Forms.Button bt_enterFolderForSpam;
    }
}