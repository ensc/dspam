namespace Engine.GUI
{
    partial class MainForm
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
            this.tB_advertising = new System.Windows.Forms.TextBox();
            this.bt_thankYou = new System.Windows.Forms.Button();
            this.bt_checkForUpdate = new System.Windows.Forms.Button();
            this.bt_sendLogToDeveloper = new System.Windows.Forms.Button();
            this.bt_closeForm = new System.Windows.Forms.Button();
            this.bt_TrainingForm = new System.Windows.Forms.Button();
            this.bt_AktionForm = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // tB_advertising
            // 
            this.tB_advertising.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.tB_advertising.Location = new System.Drawing.Point(12, 12);
            this.tB_advertising.Multiline = true;
            this.tB_advertising.Name = "tB_advertising";
            this.tB_advertising.Size = new System.Drawing.Size(531, 141);
            this.tB_advertising.TabIndex = 0;
            this.tB_advertising.Text = "this textbox contains some stuff";
            // 
            // bt_thankYou
            // 
            this.bt_thankYou.Location = new System.Drawing.Point(12, 159);
            this.bt_thankYou.Name = "bt_thankYou";
            this.bt_thankYou.Size = new System.Drawing.Size(173, 23);
            this.bt_thankYou.TabIndex = 1;
            this.bt_thankYou.Text = "bt_thankYou";
            this.bt_thankYou.UseVisualStyleBackColor = true;
            this.bt_thankYou.Click += new System.EventHandler(this.bt_thankYou_Click);
            // 
            // bt_checkForUpdate
            // 
            this.bt_checkForUpdate.Location = new System.Drawing.Point(191, 159);
            this.bt_checkForUpdate.Name = "bt_checkForUpdate";
            this.bt_checkForUpdate.Size = new System.Drawing.Size(173, 23);
            this.bt_checkForUpdate.TabIndex = 2;
            this.bt_checkForUpdate.Text = "bt_checkForUpdate";
            this.bt_checkForUpdate.UseVisualStyleBackColor = true;
            this.bt_checkForUpdate.Click += new System.EventHandler(this.bt_checkForUpdate_Click);
            // 
            // bt_sendLogToDeveloper
            // 
            this.bt_sendLogToDeveloper.Location = new System.Drawing.Point(370, 159);
            this.bt_sendLogToDeveloper.Name = "bt_sendLogToDeveloper";
            this.bt_sendLogToDeveloper.Size = new System.Drawing.Size(173, 23);
            this.bt_sendLogToDeveloper.TabIndex = 3;
            this.bt_sendLogToDeveloper.Text = "bt_sendLogToDeveloper";
            this.bt_sendLogToDeveloper.UseVisualStyleBackColor = true;
            this.bt_sendLogToDeveloper.Click += new System.EventHandler(this.bt_sendLogToDeveloper_Click);
            // 
            // bt_closeForm
            // 
            this.bt_closeForm.Location = new System.Drawing.Point(468, 201);
            this.bt_closeForm.Name = "bt_closeForm";
            this.bt_closeForm.Size = new System.Drawing.Size(75, 23);
            this.bt_closeForm.TabIndex = 4;
            this.bt_closeForm.Text = "bt_closeForm";
            this.bt_closeForm.UseVisualStyleBackColor = true;
            this.bt_closeForm.Click += new System.EventHandler(this.bt_closeForm_Click);
            // 
            // bt_TrainingForm
            // 
            this.bt_TrainingForm.Location = new System.Drawing.Point(12, 201);
            this.bt_TrainingForm.Name = "bt_TrainingForm";
            this.bt_TrainingForm.Size = new System.Drawing.Size(173, 23);
            this.bt_TrainingForm.TabIndex = 5;
            this.bt_TrainingForm.Text = "bt_TrainingForm";
            this.bt_TrainingForm.UseVisualStyleBackColor = true;
            this.bt_TrainingForm.Click += new System.EventHandler(this.bt_TrainingForm_Click);
            // 
            // bt_AktionForm
            // 
            this.bt_AktionForm.Location = new System.Drawing.Point(191, 201);
            this.bt_AktionForm.Name = "bt_AktionForm";
            this.bt_AktionForm.Size = new System.Drawing.Size(173, 23);
            this.bt_AktionForm.TabIndex = 6;
            this.bt_AktionForm.Text = "bt_AktionForm";
            this.bt_AktionForm.UseVisualStyleBackColor = true;
            this.bt_AktionForm.Click += new System.EventHandler(this.bt_AktionForm_Click);
            // 
            // MainForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(557, 237);
            this.Controls.Add(this.bt_AktionForm);
            this.Controls.Add(this.bt_TrainingForm);
            this.Controls.Add(this.bt_closeForm);
            this.Controls.Add(this.bt_sendLogToDeveloper);
            this.Controls.Add(this.bt_checkForUpdate);
            this.Controls.Add(this.bt_thankYou);
            this.Controls.Add(this.tB_advertising);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedToolWindow;
            this.Name = "MainForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "MainForm";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.TextBox tB_advertising;
        private System.Windows.Forms.Button bt_thankYou;
        private System.Windows.Forms.Button bt_checkForUpdate;
        private System.Windows.Forms.Button bt_sendLogToDeveloper;
        private System.Windows.Forms.Button bt_closeForm;
        private System.Windows.Forms.Button bt_TrainingForm;
        private System.Windows.Forms.Button bt_AktionForm;
    }
}