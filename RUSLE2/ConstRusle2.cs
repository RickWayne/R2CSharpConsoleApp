namespace Utilities.Static.Const
{
    using System.Collections.Generic;
    using System.ComponentModel;
    using System.ComponentModel.DataAnnotations;

    public static partial class ConstRusle2
    {
        public const char OPS_DELIMITER = ':'; // (char)7
        //01Oct21 JWW Insert manure operation
        public enum OpsReferenceDate
        {
            FirstOperation = 0,
            SeasonDefaultDate,
            FirstDayOfSeason,
            AprilFirst = 41,
            JuneFifth = 65,
            AugustFifth = 85,
            January15 = 115
        }
        public enum OpsValueType
        {
            NoValue = 0,
            [Display(Name = "  Yield goal")]
            YieldGoal,
            [Display(Name = "  Residue")]
            Residue
        }

        public const string CRLF = "\r\n";
        public const string TAB = "\t";
        public const string CRLF2 = CRLF + CRLF;
        public const string TAB2 = TAB + TAB;

        public const string PROFILES_DEFAULT = @"profiles\default";

        public const string OP_DATE = "OP_DATE";
        public const string OP_PTR = "OP_PTR";
        public const string RES_ADDED = "RES_ADDED";
        public const string EXT_RES_PTR = "EXT_RES_PTR";

        public const string MAN_BASE_PTR = "MAN_BASE_PTR";

        public const string MAN_OP_VEG_NUM_HARV_UNITS = "MAN_OP_VEG_NUM_HARV_UNITS";
        public const string MAN_LAYER = "MAN_LAYER";
        public const string MAN_PTR = "MAN_PTR";

        public const string CLIMATE_PTR = "CLIMATE_PTR";
        public const string CLIMATE_VALUE = @"climates\USA\Wisconsin\XXXX County"; //replace XXXX with actual county name

        //13Aug21 JWW From Dalmo
        //We changed the default option for SINGLE_STORM_MODEL_CHOICE to  SINGLE_STORM_MODEL_CHOICE_STORM_SEQUENCE a couple of years ago.
        //In the interface the parameter is called “How model storms?”
        //If you want your application to behave as the default RUSLE2 application, you can simply omit this setting.
        //13Aug21 LWG The answer is yes – we want to use the default setting 
        //public const string SINGLE_STORM_MODEL_CHOICE          = "SINGLE_STORM_MODEL_CHOICE";
        //public const string SINGLE_STORM_MODEL_CHOICE_NORMAL   = "SINGLE_STORM_MODEL_CHOICE_NORMAL";
        //public const string SINGLE_STORM_MODEL_CHOICE_STORM_SEQUENCE = "SINGLE_STORM_MODEL_CHOICE_STORM_SEQUENCE";

        public const string SLOPE_NET_SIZE_CLASS_MASS_P_AREA = "SLOPE_NET_SIZE_CLASS_MASS_P_AREA";

        //clay, silt, sand, smAgg, lgAgg
        public const string SLOPE_YEARLY_NET_SIZE_CLASS_MASS_P_AREA = "SLOPE_YEARLY_NET_SIZE_CLASS_MASS_P_AREA";
        public const string SLOPE_YEARLY_DEGRAD = "SLOPE_YEARLY_DEGRAD";
        public const string SLOPE_DEGRAD = "SLOPE_DEGRAD";
        public const string SLOPE_DELIVERY = "SLOPE_DELIVERY";
        public const string MAN_PIECE_SOIL_LOSS = "MAN_PIECE_SOIL_LOSS";
        public const string SLOPE_STEEP = "SLOPE_STEEP";
        public const string SLOPE_HYPOT = "SLOPE_HYPOT";
        public const string SLOPE_HORIZ = "SLOPE_HORIZ";
        public const string SLOPE_SIM_DAY_DETACH_RATE = "SLOPE_SIM_DAY_DETACH_RATE";
        public const string SLOPE_DETACH = "SLOPE_DETACH";

        public const string SOIL_PTR = "SOIL_PTR";

        public const string OP_INSERT = "#INSERT";

        //12Aug21 JWW Remote Down attributes - Remote Down is used to protect against leaving managements in a altered state for the next use of the management.
        //  Dalmo/Brandon example for altering the management yield.
        //  RomeFileSetAttrValue(profileObject, "#RD:SLOPE_YIELD_INPUTS_PTR:YIELD_INPUTS_YIELD", yieldValue, index);
        //              As long as we close all open R2 files between erosion runs there is no need to worry.
        //              If we don't close all, we have to close all open managements and operations between runs.
        public const string RD_MAN_BASE_PTR = "#RD:MAN_BASE_PTR:";
        public const string RD_MAN_BASE_PTR_OP_DATE = "#RD:MAN_BASE_PTR:OP_DATE";
        public const string RD_MAN_BASE_PTR_OP_PTR = "#RD:MAN_BASE_PTR:OP_PTR";
        public const string RD_MAN_BASE_PTR_EXT_RES_PTR = "#RD:MAN_BASE_PTR:EXT_RES_PTR";
        public const string RD_MAN_BASE_PTR_RES_ADDED = "#RD:MAN_BASE_PTR:RES_ADDED";
        public const string RD_VEG_BASE_PTR_VEG_NUM_HARV_UNITS = "#RD:MAN_BASE_PTR:MAN_OP_VEG_NUM_HARV_UNITS";

        public const string RESULTS_DOUBLE_FORMAT = "########0.0#####";

        // 17Dec22 JWW Add consts
        // 28Dec22 JWW Why don't these start with #RD ?? Only when using a PTR ??
        // public const string RD_CROP_YEAR_OP_DATE    = "#RD:CROP_YEAR_OP_DATE";

        //04Jan23 JWW According to Laura's spreadsheet, this has to be set to yes for the last harvest in each crop year
        //              I am not doing that now but I'm getting the same numbers as she is.
        // TODO rw 8 Jun 2023 Maybe something to look into? According to the outputs file there are two things in
        // play: CROP_YEAR_END_SET_CHOICE and CROP_YEAR_END_START. Maybe both have to be set?
        public const string RD_MAN_OP_VEG_CROP_YR_END_START = "#RD:MAN_PTR:MAN_OP_VEG_CROP_YR_END_START";

        // 17Dec22
        public const string MAN_DURATION = "MAN_DURATION";
        public const string NET_C_FACTOR = "NET_C_FACTOR";
        public const string RD_CROP_YEAR_NUM_YRS = "#RD:CROP_YEAR_PTR:CROP_YEAR_NUM_YRS";
        public const string RD_CROP_YEAR_END_START = "#RD:CROP_YEAR_PTR:CROP_YEAR_END_START";
        //Start Date 
        public const string RD_CROP_YEAR_START_DATE = "#RD:CROP_YEAR_PTR:CROP_YEAR_START_DATE";
        //End Date 
        public const string RD_CROP_YEAR_END_DATE = "#RD:CROP_YEAR_PTR:CROP_YEAR_END_DATE";

        // 29Dec22 JWW 
        public const string RD_CROP_YEAR_END_START_CHOICE = "#RD:CROP_YEAR_PTR:CROP_YEAR_END_START_CHOICE";
        public const string RD_MAN_BASE_ROTATION_CHOICE = "#RD:MAN_BASE_PTR:MAN_BASE_ROTATION_CHOICE";
        public const string RD_MAN_CROP_YEAR_NUM_YRS = "#RD:MAN_PTR:MAN_CROP_YEAR_NUM_YRS";
        // 28Dec22 JWW Add #END


        //Crop
        // 29Dec22 JWW change RD: to #RD:CROP_YEAR_PTR:
        public const string RD_CROP_YEAR_CROP_NAME = "#RD:CROP_YEAR_PTR:CROP_YEAR_CROP_NAME";
        public const string RD_CROP_YEAR_YIELD_STR = "#RD:CROP_YEAR_PTR:CROP_YEAR_YIELD_STR";
        public const string RD_CROP_YEAR_VEG_NAME = "#RD:CROP_YEAR_PTR:CROP_YEAR_VEG_NAME";

        //STIR
        //  29Dec22 JWW was '::CROP_YEAR_STIR'
        public const string RD_CROP_YEAR_STIR = "#RD:CROP_YEAR_PTR:CROP_YEAR_STIR";
        public const string RD_CROP_YEAR_OP_STIR = "#RD:CROP_YEAR_PTR:CROP_YEAR_OP_STIR";

        public const string RD_CROP_YEAR_OP_NAME = "#RD:CROP_YEAR_PTR:CROP_YEAR_OP_NAME";
        public const string RD_CROP_YEAR_EXT_RES_NAME = "#RD:CROP_YEAR_PTR:CROP_YEAR_EXT_RES_NAME";
        public const string RD_CROP_YEAR_EXT_RES_AMT = "#RD:CROP_YEAR_PTR:CROP_YEAR_EXT_RES_AMT";
        // 17Dec22 ===========================



        public const string SLOPE_YIELD_INPUTS_PTR = "SLOPE_YIELD_INPUTS_PTR";

        //12Jan22 JWW Added for dump operations
        public const string YIELD_INPUTS_YIELD = "YIELD_INPUTS_YIELD";
        public const string RD_MAN_BASE_PTR_MAN_OP_VEG_NUM_HARV_UNITS = "#RD:MAN_BASE_PTR:MAN_OP_VEG_NUM_HARV_UNITS";

        public const string RD_SLOPE_YIELD_INPUTS_PTR_YIELD_INPUTS_YIELD = "#RD:SLOPE_YIELD_INPUTS_PTR:YIELD_INPUTS_YIELD";
        public const string RD_SOIL_COND_INDEX_PTR_SOIL_COND_INDEX_RESULT = "#RD:SOIL_COND_INDEX_PTR:SOIL_COND_INDEX_RESULT";
        public const string RD_SOIL_COND_INDEX_PTR_SOIL_COND_INDEX_STIR_VAL = "#RD:SOIL_COND_INDEX_PTR:SOIL_COND_INDEX_STIR_VAL";
        public const string RD_SLOPE_ROT_BUILD_NUM_ROTATION_BUILDER_MANS_INSERT = "#RD:SLOPE_ROT_BUILD_SUB_OBJ_PTR:NUM_ROTATION_BUILDER_MANS";
        public const string RD_SLOPE_ROT_BUILD_NUM_ROTATION_BUILDER_MANS_PTRS = "#RD:SLOPE_ROT_BUILD_SUB_OBJ_PTR:ROTATION_BUILDER_MAN_PTRS";
        public const string RD_SLOPE_ROT_BUILD_APPLY_YES_NO = "#RD:SLOPE_ROT_BUILD_SUB_OBJ_PTR:ROT_BUILD_APPLY";
        public const string RD_SLOPE_ROT_BUILD_CHOICE_YES_NO = "#RD:MAN_BASE_PTR:MAN_BASE_ROTATION_CHOICE";

        public const string CHAR_SOIL_T_VALUE = "CHAR_SOIL_T_VALUE";

        public const string CONTOUR_SYSTEM_PTR = "CONTOUR_SYSTEM_PTR";
        public const string CONTOUR_SYSTEM_PATH_A = @"contour-systems\a. rows up-and-down hill";
        public const string CONTOUR_SYSTEM_PATH_B = @"contour-systems\b. absolute row grade  0.1 percent";
        public const string CONTOUR_SYSTEM_PATH_C = @"contour-systems\c. perfect contouring no row grade";
        public const string CONTOUR_SYSTEM_PATH_E = @"contour-systems\e. relative row grade 10 percent of slope grade";
        public const string CONTOUR_BUFFER_DEFAULT = @"strip-barrier-systems\Contour Buffer Strips\Actual width 15 ft\2-Cool season grass buffers not at end 15 feet wide";
        public const string CONTOUR_BUFFER_15_FT = @"strip-barrier-systems\Contour Buffer Strips\Actual width 15 ft\1-Cool season grass buffer midslope 15 feet wide";
        public const string FILTER_STRIP_BUFFER_30_FT = @"strip-barrier-systems\Filter strips\Actual width\30-ft Cool season grass filter strip";

            public const string FILTER_STRIP_DEFAULT = @"strip-barrier-systems\Filter strips\Actual width\30-ft Cool season grass filter strip";

        public const string STRIP_BARRIER_SYSTEM_PTR = "STRIP_BARRIER_SYSTEM_PTR";
        public const string STRIP_BARRIER_SYSTEM_PATH_STRIP_0_2 = @"strip-barrier-systems\Strip cropping\2strip rotational 0-2";
        public const string STRIP_BARRIER_SYSTEM_PATH_STRIP_0_3 = @"strip-barrier-systems\Strip cropping\2strip rotational 0-3";
        public const string STRIP_BARRIER_SYSTEM_PATH_CONTOUR_BUFFER = @"strip-barrier-systems\Contour Buffer Strips\Actual width 15 ft\1-Cool season grass buffer midslope 15 feet wide";
        public const string STRIP_BARRIER_MAN_PATH_COOL_SEASON_GRASS = @"managements\Strip/Barrier Managements\Cool season grass; not harvested"; // forsnap";
        public const string STRIP_BARRIER_MAN_PATH_COOL_SEASON_GRASS_FOR_SNAP = @"managements\Strip/Barrier Managements\Cool season grass; not harvestedforsnap";
        public const string RD_STRIP_BARRIER_MANAGEMENT_OP_DATE = "#RD:STRIP_BARRIER_SYSTEM_PTR:#RD:STRIP_BARRIER_MANAGEMENT:OP_DATE";
        public const string STRIP_BARRIER_MANAGEMENT_OP_DATE_VALUE = "4/1/0";

        public const string DEFAULT_DATE = "1/1/0";


        public const string MAN_PIECE_SED_DELIVERY = "MAN_PIECE_SED_DELIVERY";
        public const string YEARLY_DATES = "YEARLY_DATES";

        public const string SEG_YEARLY_SED_CLASS_MASS_P_AREA = "SEG_YEARLY_SED_CLASS_MASS_P_AREA";
        public const string SEG_YEARLY_SOIL_LOSS = "SEG_YEARLY_SOIL_LOSS";
        public const string SEG_YEARLY_SED_DEL = "SEG_YEARLY_SED_DEL";
        public const string SEG_YEARLY_DEGRAD = "SEG_YEARLY_DEGRAD";
        public const string SEG_HYPOT = "SEG_HYPOT";
        public const string SEG_SIM_DAY_CN = "SEG_SIM_DAY_CN";
        public const string SEG_SIM_DAY_NET_SURF_COV = "SEG_SIM_DAY_NET_SURF_COV";
        public const string SEG_SIM_DAY_NET_CANOPY_COVER = "SEG_SIM_DAY_NET_CANOPY_COVER";


        public const string RD_WI_SNAP_FROST_FREE_CN = "#RD:WI_SNAP_PTR:WI_SNAP_FROST_FREE_CN";          // r2 base index
        public const string RD_WI_SNAP_FROST_FREE_YEARLY_CN = "#RD:WI_SNAP_PTR:WI_SNAP_FROST_FREE_YEARLY_CN";   // year index
        public const string R2_ATTR_NOVALUE = "#ENTRY_NONE";

        //02Mar22 JWW NO OPERATION
        public const string ZONE1_SIM_START = "10/15/1";
        public const string ZONE4_SIM_START = "11/1/1";
        public const string ZONE1_SIM_END = "10/14/1";
        public const string ZONE4_SIM_END = "10/31/1";
        public const string NO_OPERATION = @"operations\No operation";


        //14Jul21 LWG Tillage start and end dates obsolete.
        //  Insert tillage before first operatio according to rules on the first tab in: 
        //          manure operations insertion dates table working.xlsx
        //13Jul21 JWW CMZ 01 Season start/end dates
        public const string R2_OPERATION_DATE_FORMAT = "M/d/y";
        public const string DATE_FORMAT = "yyyy-MM-dd";


        public const int R2_BASE_INDEX = 0;
        public const int R2_CLAY_INDEX = 0;
        public const int R2_SILT_INDEX = 1;
        public const int R2_SAND_INDEX = 2;
        public const int R2_SMALLAGG_INDEX = 3;
        public const int R2_LARGEAGG_INDEX = 4;

        public const string R2_MANAGEMENTS_BASE = @"managements\";

        /// <summary>
        /// Types of Erosion model runs
        /// </summary>
        public enum ErosionModelRunType
        {
            /// <summary>
            /// Normal, run RUSLE2 as rotation
            /// </summary>
            Normal,
            /// <summary>
            /// 
            /// </summary>
            ErosionOnly,
            /// <summary>
            /// Run as RUSLE2 sequence
            /// </summary>
            Sequence,
            /// <summary>
            /// Run as RUSLE2 sequence using ag soil
            /// </summary>
            Trade
        }
        public enum R2SedDeliveryIndex
        {
            Silt = 1,
            Sand,
            SmallAgg,
            LargeAgg
        }
        public enum CropManagementNameParts
        {
            Management = 0,
            Zone,
            Season,
            Crop,
            Tillage
        }

        public enum CropManagementZone
        {
            //21Jul21 JWW Add description and display name attributes
            //      Description could be how Laura uses it in the flow diagrams - CMZ-01 and CMZ-04
            //      Display name is how it is used in the GDB management paths
            NotSet = 0,
            [Description("CMZ-01")]
            [Display(Name = "CMZ 01")]
            CMZ01 = 1,
            [Description("CMZ-04")]
            [Display(Name = "CMZ 04")]
            CMZ04 = 4
        }
        public enum GrowingDegreeDayZone
        {
            Northern = 1,
            NorthTransition,
            SouthTransition,
            Southern
        }

        //28Dec22 JWW Add Display name for remote down attributes

        public enum ErosionResultItem
        {
            MAN_PIECE_SED_DELIVERY = 0,     //FilterStrip==1; get AvgSoilLoss = (segIndex -1)
            MAN_PIECE_SOIL_LOSS,            // 16Dec22 JWW Restore annual results
            SLOPE_DEGRAD,                   //FilterStrip!=1; get AvgSoilLoss = R2_BASE_INDEX
            SLOPE_DELIVERY,
            MAN_DURATION,                   //R2_BASE_INDEX
            [Display(Name = "#RD:WI_SNAP_PTR:WI_SNAP_FROST_FREE_YEARLY_CN")]
            WI_SNAP_FROST_FREE_YEARLY_CN,   //R2_BASE_INDEX
            [Display(Name = "#RD:WI_SNAP_PTR:WI_SNAP_FROST_FREE_CN")]
            WI_SNAP_FROST_FREE_CN,          //R2_BASE_INDEX
            SLOPE_YEARLY_DEGRAD,            //R2_BASE_INDEX
            SEG_YEARLY_DEGRAD,              //R2_BASE_INDEX
            SEG_YEARLY_SED_CLASS_MASS_P_AREA, //17Dec22
            SEG_YEARLY_SOIL_LOSS,           //R2_BASE_INDEX
            SEG_YEARLY_SED_DEL,             //R2_BASE_INDEX
            [Display(Name = "#RD:SOIL_COND_INDEX_PTR:SOIL_COND_INDEX_RESULT")]
            SOIL_COND_INDEX_RESULT,
            [Display(Name = "#RD:SOIL_COND_INDEX_PTR:SOIL_COND_INDEX_STIR_VAL")]
            SOIL_COND_INDEX_STIR_VAL,
            [Display(Name = "#RD:SOIL_COND_INDEX_PTR:SOIL_COND_INDEX_STIR_VALUE")]
            SOIL_COND_INDEX_STIR_VALUE,
            //01Mar22 JWW Add more results
            NET_C_FACTOR,
            // 03Jan23 JWW According the Laura's spreadsheet 
            [Display(Name = "#RD:MAN_PTR:MAN_CROP_YEAR_STIR")]
            MAN_CROP_YEAR_STIR,
            CROP_YEAR_STIR,
            YEARLY_DATES,                       //17Dec22
            SEG_SIM_DAY_FRAC_SURF_COVER,
            SEG_SIM_YEARLY_FRAC_SURF_COVER,     //17Dec22
            SEG_SIM_DAY_CANOPY_COVER,
            SEG_SIM_DAY_CN,
            SLOPE_NET_SIZE_CLASS_MASS_P_AREA,
            SLOPE_YEARLY_NET_SIZE_CLASS_MASS_P_AREA, //17Dec22
            SLOPE_YEARLY_NET_SIZE_CLASS_MASS_WIDTH_AREA
            , R2_CLAY_INDEX
            , R2_SILT_INDEX
            , R2_SAND_INDEX
            , R2_SMALLAGG_INDEX
            , R2_LARGEAGG_INDEX
        }
        public static List<string> GetMonthsOfYear(bool abbr)
        {
            List<string> months;
            if (abbr) months = new List<string>() { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
            else months = new List<string>() { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
            return months;
        }
    }

}
