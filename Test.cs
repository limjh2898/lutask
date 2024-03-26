using ExcelAddIn_Test.Model;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using System.Text.Json;

namespace ExcelAddIn_Test.Verfy
{
    public class RowData
    {
        public UInt64 Version { get; set; } = 0;
        public List<Dictionary<string, string>> Data { get; set; } = new List<Dictionary<string, string>>();
    }
    public class Verifier
    {
        private static Dictionary<string, ForienKeyCheckInfo> CheckInfos = new Dictionary<string, ForienKeyCheckInfo>();

        private class FkValue
        {
            public string FkField;
            public HashSet<string> Values;
        }

        private class ForienKeyCheckInfo
        {
            public Dictionary<string,HashSet<string>> Fks = new Dictionary<string, HashSet<string>>();
        }

        private bool PrepareRefDatas(TableModel model)
        {
            CheckInfos.Clear();
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

                if (CheckInfos.TryGetValue(tableModel, out var info) == false)
                {
                    info = new ForienKeyCheckInfo();
                    CheckInfos.Add(tableModel, info);
                }

                if (info.Fks.ContainsKey(fkField))
                    continue;

                info.Fks.Add(fkField, new HashSet<string>());
            }
        }

        public bool Verify(RowData rowDatas, TableModel model)
        {
            PrepareRefDatas(model);
               

            Parallel.ForEach(fileMatchs, match =>
            {
                var fk = match.Item1;
                string filePath = match.Item2;

                FileStream stream = new FileStream(match.Item2, FileMode.Open, FileAccess.Read);
                JsonDocument jdom = JsonDocument.Parse(stream);
                stream.Close();

                JsonElement jroot = jdom.RootElement;
                var datas = jroot.GetProperty("Data")
                    .EnumerateArray()
                    .Cast<JsonElement?>()
                    .ToList();

                HashSet<string> fkValues = new HashSet<string>();
                for (int i = 0; i < datas.Count; i++)
                {
                    var data = datas[i];
                    if (data.HasValue == false)
                        continue;

                    string value = data.Value.GetProperty(fk.FkField).GetRawText();
                    fkValues.Add(value);
                }

                for (int i = 0; i < rowDatas.Data.Count; i++)
                {
                    var rowData = rowDatas.Data[i];
                    if (rowData.TryGetValue(fk.Field, out var rowValue) == false)
                        continue;

                    if (fkValues.Contains(rowValue))

                }
            });
        }
    }
}

