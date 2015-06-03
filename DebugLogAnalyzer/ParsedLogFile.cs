using System;
using System.Collections.Generic;
using System.Text;
using System.IO;

namespace DebugLogAnalyzer
{
    class ParsedLogFile
    {
        public enum SCSIOP
        {
            SCSIOP_TEST_UNIT_READY = 0x00
             ,
            SCSIOP_REZERO_UNIT = 0x01
                ,
            SCSIOP_REWIND = 0x01
                ,
            SCSIOP_REQUEST_BLOCK_ADDR = 0x02
                ,
            SCSIOP_REQUEST_SENSE = 0x03
                ,
            SCSIOP_FORMAT_UNIT = 0x04
                ,
            SCSIOP_READ_BLOCK_LIMITS = 0x05
                ,
            SCSIOP_REASSIGN_BLOCKS = 0x07
                ,
            SCSIOP_INIT_ELEMENT_STATUS = 0x07
                ,
            SCSIOP_READ6 = 0x08
                ,
            SCSIOP_RECEIVE = 0x08
                ,
            SCSIOP_WRITE6 = 0x0A
                ,
            SCSIOP_PRINT = 0x0A
                ,
            SCSIOP_SEND = 0x0A
                ,
            SCSIOP_SEEK6 = 0x0B
                ,
            SCSIOP_TRACK_SELECT = 0x0B
                ,
            SCSIOP_SLEW_PRINT = 0x0B
                ,
            SCSIOP_SET_CAPACITY = 0x0B // tape
                ,
            SCSIOP_SEEK_BLOCK = 0x0C
                ,
            SCSIOP_PARTITION = 0x0D
                ,
            SCSIOP_READ_REVERSE = 0x0F
                ,
            SCSIOP_WRITE_FILEMARKS = 0x10
                ,
            SCSIOP_FLUSH_BUFFER = 0x10
                ,
            SCSIOP_SPACE = 0x11
                ,
            SCSIOP_INQUIRY = 0x12
                ,
            SCSIOP_VERIFY6 = 0x13
                ,
            SCSIOP_RECOVER_BUF_DATA = 0x14
                ,
            SCSIOP_MODE_SELECT = 0x15
                ,
            SCSIOP_RESERVE_UNIT = 0x16
                ,
            SCSIOP_RELEASE_UNIT = 0x17
                ,
            SCSIOP_COPY = 0x18
                ,
            SCSIOP_ERASE = 0x19
                ,
            SCSIOP_MODE_SENSE = 0x1A
                ,
            SCSIOP_START_STOP_UNIT = 0x1B
                ,
            SCSIOP_LOAD_UNLOAD = 0x1B
                ,
            SCSIOP_RECEIVE_DIAGNOSTIC = 0x1C
                ,
            SCSIOP_SEND_DIAGNOSTIC = 0x1D
                ,
            SCSIOP_MEDIUM_REMOVAL = 0x1E

                // 10-byte commands
                ,
            SCSIOP_READ_FORMATTED_CAPACITY = 0x23
                ,
            SCSIOP_READ_CAPACITY = 0x25
                ,
            SCSIOP_READ = 0x28
                ,
            SCSIOP_WRITE = 0x2A
                ,
            SCSIOP_SEEK = 0x2B
                ,
            SCSIOP_LOCATE = 0x2B
                ,
            SCSIOP_POSITION_TO_ELEMENT = 0x2B
                ,
            SCSIOP_WRITE_VERIFY = 0x2E
                ,
            SCSIOP_VERIFY = 0x2F
                ,
            SCSIOP_SEARCH_DATA_HIGH = 0x30
                ,
            SCSIOP_SEARCH_DATA_EQUAL = 0x31
                ,
            SCSIOP_SEARCH_DATA_LOW = 0x32
                ,
            SCSIOP_SET_LIMITS = 0x33
                ,
            SCSIOP_READ_POSITION = 0x34
                ,
            SCSIOP_SYNCHRONIZE_CACHE = 0x35
                ,
            SCSIOP_COMPARE = 0x39
                ,
            SCSIOP_COPY_COMPARE = 0x3A
                ,
            SCSIOP_WRITE_DATA_BUFF = 0x3B
                ,
            SCSIOP_READ_DATA_BUFF = 0x3C
                ,
            SCSIOP_WRITE_LONG = 0x3F
                ,
            SCSIOP_CHANGE_DEFINITION = 0x40
                ,
            SCSIOP_WRITE_SAME = 0x41
                ,
            SCSIOP_READ_SUB_CHANNEL = 0x42
                ,
            SCSIOP_READ_TOC = 0x43
                ,
            SCSIOP_READ_HEADER = 0x44
                ,
            SCSIOP_REPORT_DENSITY_SUPPORT = 0x44 // tape
                ,
            SCSIOP_PLAY_AUDIO = 0x45
                ,
            SCSIOP_GET_CONFIGURATION = 0x46
                ,
            SCSIOP_PLAY_AUDIO_MSF = 0x47
                ,
            SCSIOP_PLAY_TRACK_INDEX = 0x48
                ,
            SCSIOP_PLAY_TRACK_RELATIVE = 0x49
                ,
            SCSIOP_GET_EVENT_STATUS = 0x4A
                ,
            SCSIOP_PAUSE_RESUME = 0x4B
                ,
            SCSIOP_LOG_SELECT = 0x4C
                ,
            SCSIOP_LOG_SENSE = 0x4D
                ,
            SCSIOP_STOP_PLAY_SCAN = 0x4E
                ,
            SCSIOP_XDWRITE = 0x50
                ,
            SCSIOP_XPWRITE = 0x51
                ,
            SCSIOP_READ_DISK_INFORMATION = 0x51
                ,
            SCSIOP_READ_DISC_INFORMATION = 0x51 // proper use of disc over disk
                ,
            SCSIOP_READ_TRACK_INFORMATION = 0x52
                ,
            SCSIOP_XDWRITE_READ = 0x53
                ,
            SCSIOP_RESERVE_TRACK_RZONE = 0x53
                ,
            SCSIOP_SEND_OPC_INFORMATION = 0x54 // optimum power calibration
                ,
            SCSIOP_MODE_SELECT10 = 0x55
                ,
            SCSIOP_RESERVE_UNIT10 = 0x56
                ,
            SCSIOP_RESERVE_ELEMENT = 0x56
                ,
            SCSIOP_RELEASE_UNIT10 = 0x57
                ,
            SCSIOP_RELEASE_ELEMENT = 0x57
                ,
            SCSIOP_REPAIR_TRACK = 0x58
                ,
            SCSIOP_MODE_SENSE10 = 0x5A
                ,
            SCSIOP_CLOSE_TRACK_SESSION = 0x5B
                ,
            SCSIOP_READ_BUFFER_CAPACITY = 0x5C
                ,
            SCSIOP_SEND_CUE_SHEET = 0x5D
                ,
            SCSIOP_PERSISTENT_RESERVE_IN = 0x5E
                ,
            SCSIOP_PERSISTENT_RESERVE_OUT = 0x5F

                // 12-byte commands
                ,
            SCSIOP_REPORT_LUNS = 0xA0
                ,
            SCSIOP_BLANK = 0xA1
                ,
            SCSIOP_ATA_PASSTHROUGH12 = 0xA1
                ,
            SCSIOP_SEND_EVENT = 0xA2
                ,
            SCSIOP_SEND_KEY = 0xA3
                ,
            SCSIOP_MAINTENANCE_IN = 0xA3
                ,
            SCSIOP_REPORT_KEY = 0xA4
                ,
            SCSIOP_MAINTENANCE_OUT = 0xA4
                ,
            SCSIOP_MOVE_MEDIUM = 0xA5
                ,
            SCSIOP_LOAD_UNLOAD_SLOT = 0xA6
                ,
            SCSIOP_EXCHANGE_MEDIUM = 0xA6
                ,
            SCSIOP_SET_READ_AHEAD = 0xA7
                ,
            SCSIOP_MOVE_MEDIUM_ATTACHED = 0xA7
                ,
            SCSIOP_READ12 = 0xA8
                ,
            SCSIOP_GET_MESSAGE = 0xA8
                ,
            SCSIOP_SERVICE_ACTION_OUT12 = 0xA9
                ,
            SCSIOP_WRITE12 = 0xAA
                ,
            SCSIOP_SEND_MESSAGE = 0xAB
                ,
            SCSIOP_SERVICE_ACTION_IN12 = 0xAB
                ,
            SCSIOP_GET_PERFORMANCE = 0xAC
                ,
            SCSIOP_READ_DVD_STRUCTURE = 0xAD
                ,
            SCSIOP_WRITE_VERIFY12 = 0xAE
                ,
            SCSIOP_VERIFY12 = 0xAF
                ,
            SCSIOP_SEARCH_DATA_HIGH12 = 0xB0
                ,
            SCSIOP_SEARCH_DATA_EQUAL12 = 0xB1
                ,
            SCSIOP_SEARCH_DATA_LOW12 = 0xB2
                ,
            SCSIOP_SET_LIMITS12 = 0xB3
                ,
            SCSIOP_READ_ELEMENT_STATUS_ATTACHED = 0xB4
                ,
            SCSIOP_REQUEST_VOL_ELEMENT = 0xB5
                ,
            SCSIOP_SEND_VOLUME_TAG = 0xB6
                ,
            SCSIOP_SET_STREAMING = 0xB6 // C/DVD
                ,
            SCSIOP_READ_DEFECT_DATA = 0xB7
                ,
            SCSIOP_READ_ELEMENT_STATUS = 0xB8
                ,
            SCSIOP_READ_CD_MSF = 0xB9
                ,
            SCSIOP_SCAN_CD = 0xBA
                ,
            SCSIOP_REDUNDANCY_GROUP_IN = 0xBA
                ,
            SCSIOP_SET_CD_SPEED = 0xBB
                ,
            SCSIOP_REDUNDANCY_GROUP_OUT = 0xBB
                ,
            SCSIOP_PLAY_CD = 0xBC
                ,
            SCSIOP_SPARE_IN = 0xBC
                ,
            SCSIOP_MECHANISM_STATUS = 0xBD
                ,
            SCSIOP_SPARE_OUT = 0xBD
                ,
            SCSIOP_READ_CD = 0xBE
                ,
            SCSIOP_VOLUME_SET_IN = 0xBE
                ,
            SCSIOP_SEND_DVD_STRUCTURE = 0xBF
                ,
            SCSIOP_VOLUME_SET_OUT = 0xBF
                ,
            SCSIOP_INIT_ELEMENT_RANGE = 0xE7

                // 16-byte commands
                ,
            SCSIOP_XDWRITE_EXTENDED16 = 0x80 // disk
                ,
            SCSIOP_WRITE_FILEMARKS16 = 0x80 // tape
                ,
            SCSIOP_REBUILD16 = 0x81 // disk
                ,
            SCSIOP_READ_REVERSE16 = 0x81 // tape
                ,
            SCSIOP_REGENERATE16 = 0x82 // disk
                ,
            SCSIOP_EXTENDED_COPY = 0x83
                ,
            SCSIOP_RECEIVE_COPY_RESULTS = 0x84
                ,
            SCSIOP_ATA_PASSTHROUGH16 = 0x85
                ,
            SCSIOP_ACCESS_CONTROL_IN = 0x86
                ,
            SCSIOP_ACCESS_CONTROL_OUT = 0x87
                ,
            SCSIOP_READ16 = 0x88
                ,
            SCSIOP_WRITE16 = 0x8A
                ,
            SCSIOP_READ_ATTRIBUTES = 0x8C
                ,
            SCSIOP_WRITE_ATTRIBUTES = 0x8D
                ,
            SCSIOP_WRITE_VERIFY16 = 0x8E
                ,
            SCSIOP_VERIFY16 = 0x8F
                ,
            SCSIOP_PREFETCH16 = 0x90
                ,
            SCSIOP_SYNCHRONIZE_CACHE16 = 0x91
                ,
            SCSIOP_SPACE16 = 0x91 // tape
                ,
            SCSIOP_LOCK_UNLOCK_CACHE16 = 0x92
                ,
            SCSIOP_LOCATE16 = 0x92 // tape
                ,
            SCSIOP_WRITE_SAME16 = 0x93
                ,
            SCSIOP_ERASE16 = 0x93 // tape
                ,
            SCSIOP_READ_CAPACITY16 = 0x9E
                ,
            SCSIOP_SERVICE_ACTION_IN16 = 0x9E
                , 
            SCSIOP_SERVICE_ACTION_OUT16 = 0x9F
        }

        public class Entry
        {
            public SCSIOP RequestType;
            public int Status;
            public DateTime StartTime, EndTime;
            public byte[] cdb;
            public int Requested, Done;
            public byte[] InData, OutData;

            public override string ToString()
            {
                return string.Format("{0} {1} - {2}", StartTime, RequestType, Status);
            }
        };

        public List<Entry> Data = new List<Entry>();

        public ParsedLogFile(string fn)
        {
            using(FileStream str = new FileStream(fn, FileMode.Open))
            while (str.Position < str.Length)
            {
                byte[] header = new byte[54];
                str.Read(header, 0, header.Length);

                uint sig1 = BitConverter.ToUInt32(header, 0);
                uint sig2 = BitConverter.ToUInt32(header, 50);

                if ((sig1 != 0xCDEB5555) || (sig2 != 0xCDEBAAAA))
                    throw new Exception("Log file is corrupt");

                Entry entry = new Entry();
                entry.RequestType = (SCSIOP)header[4];
                entry.Status = header[5];
                entry.StartTime = DateTime.FromFileTime(BitConverter.ToInt64(header, 6));
                entry.EndTime = DateTime.FromFileTime(BitConverter.ToInt64(header, 14));
                entry.cdb = new byte[16];
                Array.Copy(header, 22, entry.cdb, 0, 16);
                /*if (entry.cdb[0] != (byte)entry.RequestType)
                    throw new Exception("Log file is corrupt");*/

                entry.Requested = BitConverter.ToInt32(header, 38);
                entry.Done = BitConverter.ToInt32(header, 42);
                int saved = BitConverter.ToInt32(header, 46);

                if (saved != 0)
                {
                    entry.InData = new byte[saved];
                    entry.OutData = new byte[saved];
                    str.Read(entry.InData, 0, saved);
                    str.Read(entry.OutData, 0, saved);
                }

                Data.Add(entry);
            }
        }
    }
}
