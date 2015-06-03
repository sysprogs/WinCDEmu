using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace DebugLogAnalyzer
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();
        }

        private void button1_Click(object sender, EventArgs e)
        {
            ParsedLogFile logFile = new ParsedLogFile(@"C:\Users\Bazis\AppData\Roaming\WinCDEmu.debug\vcd00000.log");
            foreach (ParsedLogFile.Entry entry in logFile.Data)
                listBox1.Items.Add(entry);
        }
    }
}
