using SnapPlus.Models.Erosion.Entities;
using SnapPlus.Models.Erosion.Interfaces;
 /// <summary>
 /// This quickie test program runs the Rome API with nothing more than the Rusle2 project as a reference.
 /// As of the initial authoring, it agrees exactly with the RUSLE2 UI.
 /// It uses the V3 GDB right out of the data folder, making a copy to the project folder upon build.
 /// </summary>
internal partial class Program
{
    static IRusle2 rusle2 = SnapPlus.Models.Erosion.Rusle2.GetInstance(false);
    static void Main(string[] args)
    {
        string dir = Directory.GetCurrentDirectory();
        string r2Path = @"..\..\..\sample.gdb";
        Console.WriteLine($"Hello, World! I'm in '{dir}' and want '{r2Path}'");
        
        if (rusle2.OpenDatabase(r2Path) == false)
            throw new Exception($"Couldn't open database with path '{r2Path}'. Maybe set the startup path in Visual Studio?");
         if (rusle2.ProfileOpen() == false)
            throw new Exception("Could not open profile");
        SetProfAttr("CLIMATE_PTR",@"climates\USA\Wisconsin\Dane County",0);
        SetProfAttr("SOIL_PTR", @"soils\SSURGO\Dane County, Wisconsin\AsC2 Ashdale silt loam, 6 to 12 percent slopes, eroded\Ashdale Silt loam  100%", 0);
        SetProfAttr("SLOPE_STEEP","5",0);
        SetProfAttr("SLOPE_HORIZ","140",0);
        SetProfAttr("#RD:SLOPE_ROT_BUILD_SUB_OBJ_PTR:ROTATION_BUILDER_MAN_PTRS", @"managements\Cg-Sb test", 0);

        SetProfAttr("#RD:SLOPE_ROT_BUILD_SUB_OBJ_PTR:ROT_BUILD_APPLY","Yes",0);
        SetProfAttr("#RD:MAN_BASE_PTR:MAN_BASE_ROTATION_CHOICE","Yes",0);
        SetProfAttr("#RD:MAN_BASE_PTR:OP_DATE", "#INSERT", 0);
        SetProfAttr("#RD:MAN_BASE_PTR:OP_DATE", "11/1/1", 0);
        SetProfAttr("#RD:MAN_BASE_PTR:OP_PTR", @"operations\No operation", 0);
        SetProfAttr("#RD:MAN_BASE_PTR:MAN_OP_VEG_NUM_HARV_UNITS","100",4);
        SetProfAttr("#RD:MAN_BASE_PTR:MAN_OP_VEG_NUM_HARV_UNITS","30",12);
        // string duration = GetProfAttr("#RD:MAN_BASE_PTR:DURATION_IN_MAN", 0);
        string netCFactor = GetProfAttr("NET_C_FACTOR", 0);
        if (rusle2.EngineRun() == false)
            throw new Exception("Couldn't run the engine");
        Console.WriteLine($"SLOPE_DEGRAD is '{GetProfAttr("SLOPE_DEGRAD",0)}'");
        Console.WriteLine($"Net C Factor is '{netCFactor}'.");
        Console.WriteLine(DumpOpsFromR2() );
        rusle2.FilesCloseAll();
    }
    static void SetProfAttr(string item, string value, int index)
    {
        if (rusle2.ProfileSetAttrValue(item, value, index) < 0)
            throw new Exception($"Could not R2 item '{item}' value '{value}' index {index}");
    }

    static string GetProfAttr(string item, int index)
    {
        return rusle2.ProfileGetAttrValue(item,index);
    }
    static private string DumpOpsFromR2()
    {
        int nOps = rusle2.ProfileGetAttrSize("#RD:MAN_BASE_PTR:OP_DATE");
        string opStr = "";
        for (int ii = 0; ii < nOps; ii++)
        {
            opStr += $"{rusle2.ProfileGetAttrValue("#RD:MAN_BASE_PTR:OP_DATE", ii)}:{rusle2.ProfileGetAttrValue("#RD:MAN_BASE_PTR:OP_PTR", ii)} yield {rusle2.ProfileGetAttrValue("#RD:MAN_BASE_PTR:MAN_OP_VEG_NUM_HARV_UNITS", ii)} \r\n";
        }
        return opStr;
    }
}



