using System;
using System.Runtime.InteropServices;
using System.Windows.Forms;

  private void ThisAddIn_Startup(object sender, System.EventArgs e)
  {
      var datas = CsvDocument.Parse("C:\\Users\\jahyeon\\Desktop\\aaa.csv");

      InterceptKeys.SetHook(() => { });
  }

  private void ThisAddIn_Shutdown(object sender, System.EventArgs e)
  {
      InterceptKeys.ReleaseHook();
  }

namespace ExcelAddIn_Test.Util
{
    internal class InterceptKeys : IDisposable
    {
        public delegate int LowLevelKeyboardProc(int nCode, IntPtr wParam, IntPtr lParam);
        private static LowLevelKeyboardProc _proc = HookCallback;
        private static IntPtr _hookID = IntPtr.Zero;
        private static Microsoft.Office.Tools.CustomTaskPane ctpRef = null;

        private const int WH_KEYBOARD = 2;
        private const int HC_ACTION = 0;

        private static Action _action;

        public static void SetHook(Action action)
        {
            _action = action;
            _hookID = SetWindowsHookEx(WH_KEYBOARD, _proc, IntPtr.Zero, (uint)AppDomain.GetCurrentThreadId());
        }

        public static void ReleaseHook()
        {
            if (_hookID == IntPtr.Zero)
                return;

            UnhookWindowsHookEx(_hookID);
            _hookID = IntPtr.Zero;
        }

        private static int HookCallback(int nCode, IntPtr wParam, IntPtr lParam)
        {
            int PreviousStateBit = 31;
            bool KeyWasAlreadyPressed = false;
            Int64 bitmask = (Int64)Math.Pow(2, (PreviousStateBit - 1));
            if (nCode < 0)
            { 
                return (int)CallNextHookEx(_hookID, nCode, wParam, lParam);
            }
            else
            {
                if (nCode == HC_ACTION)
                {
                    Keys keyData = (Keys)wParam;
                    KeyWasAlreadyPressed = ((Int64)lParam & bitmask) > 0;
                    if (Functions.IsKeyDown(Keys.ControlKey) && keyData == Keys.E && KeyWasAlreadyPressed == false)
                    {
                        _action.Invoke();
                    }

                }

                return (int)CallNextHookEx(_hookID, nCode, wParam, lParam);
            }
        }



        [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        private static extern IntPtr SetWindowsHookEx(int idHook, LowLevelKeyboardProc lpfn, IntPtr hMod, uint dwThreadId);


        [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]

        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool UnhookWindowsHookEx(IntPtr hhk);

        [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        private static extern IntPtr CallNextHookEx(IntPtr hhk, int nCode, IntPtr wParam, IntPtr lParam);

        public void Dispose()
        {
            ReleaseHook();
        }
    }

    public class Functions
    {

        public static bool IsKeyDown(Keys keys)
        {

            return (GetKeyState((int)keys) & 0x8000) == 0x8000;

        }

        [DllImport("user32.dll")]
        static extern short GetKeyState(int nVirtKey);
    }
}



using ExcelAddIn_Test.Model;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using System.Text.Json;

namespace ExcelAddIn_Test.Verfy
{
    public static class CsvDocument
    {
        public static List<Dictionary<string, string>> Parse(string path)
        {
            if (File.Exists(path) == false)
                return null;

            List<Dictionary<string, string>> datas = new List<Dictionary<string, string>>();
            using (var reader = new StreamReader(path))
            {
                var headerLine = reader.ReadLine();
                var headers = headerLine.Split(',');

                while (reader.EndOfStream == false)
                {
                    var line = reader.ReadLine();
                    var valuese = line.Split(',');

                    if (headers.Length != valuese.Length)
                    {
                        // err log
                        continue;
                    }

                    Dictionary<string, string> data = new Dictionary<string, string>();
                    for(int i = 0; i < headers.Length; i++)
                    {
                        data.Add(headers[i], valuese[i]);
                    }

                    datas.Add(data);
                }
            }


            return datas;
        }
    }

    public class RowData
    {
        public UInt64 Version { get; set; } = 0;
        public List<Dictionary<string, string>> Data { get; set; } = new List<Dictionary<string, string>>();
    }
    public static class Verifier
    {
        private static Dictionary<string, ForienKeyCheckInfo> checkInfos = new Dictionary<string, ForienKeyCheckInfo>();

        private class FkValue
        {
            public string FkField;
            public HashSet<string> Values;
        }

        private class ForienKeyCheckInfo
        {
            public Dictionary<string, FkValue> Fks = new Dictionary<string, FkValue>();
        }

        private static void PrepareRefDatas(TableModel model)
        {
            for (int i = 0; i < model.Variables.Count; i++)
            {
                var v = model.Variables[i];
                if (v.Metas.TryGetValue("fk", out var fk) == false)
                    continue;

                var tokens = fk.Split(':').Select(x => x.Trim()).ToArray();
                if (tokens.Length != 2)
                    continue;

                string tableModel = tokens[0];
                string fkField = tokens[1];

                if (checkInfos.TryGetValue(tableModel, out var info) == false)
                {
                    info = new ForienKeyCheckInfo();
                    checkInfos.Add(tableModel, info);
                }

                if (info.Fks.ContainsKey(fkField))
                    continue;

                info.Fks.Add(fkField, new FkValue() { FkField = fkField });
            }

            Parallel.ForEach(checkInfos, item => 
            {
                string tableModel = item.Key;
                var info = item.Value;

                string filePath = "";
                string[] files = Directory.GetFiles(filePath, $"{tableModel}*");
                for(int i =  0; i < files.Length; i++)
                {
                    FileStream stream = new FileStream(files[i], FileMode.Open, FileAccess.Read);
                    JsonDocument jdom = JsonDocument.Parse(stream);
                    stream.Close();

                    JsonElement jroot = jdom.RootElement;
                    var datas = jroot.GetProperty("Data")
                        .EnumerateArray()
                        .Cast<JsonElement?>()
                        .ToList();

                    for (int j = 0; j < datas.Count; j++)
                    {
                        var data = datas[i];
                        if (data.HasValue == false)
                            continue;

                        foreach (var fkInfo in info.Fks)
                        {
                            string value = data.Value.GetProperty(fkInfo.Value.FkField).GetRawText();
                            fkInfo.Value.Values.Add(value);
                        }
                    }
                }
            });
        }

        public static bool Verify(RowData rowDatas, TableModel model)
        {
            PrepareRefDatas(model);

            for (int i = 0; i < model.Variables.Count; i++)
            {
                var v = model.Variables[i];
                if (v.Metas.TryGetValue("fk", out var fk) == false)
                    continue;

                var tokens = fk.Split(':').Select(x => x.Trim()).ToArray();
                if (tokens.Length != 2)
                    continue;

                string tableModel = tokens[0];
                string fkField = tokens[1];

                if(checkInfos.TryGetValue(tableModel, out var fkInfo) == false)
                {
                    // Not Found Table
                    return false;
                }

                if (fkInfo.Fks.TryGetValue(fkField, out var fkValue) == false)
                {
                    // Not Found Fk Field
                    return false;
                }

                for(int j = 0; j < rowDatas.Data.Count; j++)
                {
                    var data = rowDatas.Data[j];
                    if (data.TryGetValue(v.Name, out var value) == false)
                        continue;

                    if (fkValue.Values.Contains(value) == false)
                    {
                        // Not Found Fk Value
                        return false;
                    }
                }
            }

            return true;
        }
    }
}

