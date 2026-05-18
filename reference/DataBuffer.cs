using System.Collections.Concurrent;
using Serilog;
using System.Threading;
namespace laser_app_server_csharp.Processing;

static class DataBuffer
{
    private const int DATA_BUF_NUM = 2;
    private const int DATA_BUF_SIZE = 1600000;
    public static byte[][] databuf = new byte[DATA_BUF_NUM][];
    private static ConcurrentQueue<int> wr_queue = new ConcurrentQueue<int>();
    private static ConcurrentQueue<int> rd_queue = new ConcurrentQueue<int>();
    private static int wr_ptr = 0;
    private static int ptr = 0;
    private static Thread tcpThread = new Thread(TcpSocket.socketThread);


    static DataBuffer()
    {
        for (int i = 0; i < DATA_BUF_NUM; i++)
        {
            databuf[i] = new byte[DATA_BUF_SIZE];
            wr_queue.Enqueue(i);
        }
        wr_queue.TryDequeue(out wr_ptr);
    }
    
    private static void addData(ushort arg1 = 0, ushort arg2 = 0, ushort arg3 = 0,
                                ushort arg4 = 0, ushort arg5 = 0, ushort arg6 = 0,
                                ushort arg7 = 0, ushort arg8 = 0)
    {
        
        byte [] args = new byte[16]{
            Convert.ToByte(arg1 & 0xFF), Convert.ToByte(arg1 >> 8),
            Convert.ToByte(arg2 & 0xFF), Convert.ToByte(arg2 >> 8),
            Convert.ToByte(arg3 & 0xFF), Convert.ToByte(arg3 >> 8),
            Convert.ToByte(arg4 & 0xFF), Convert.ToByte(arg4 >> 8),
            Convert.ToByte(arg5 & 0xFF), Convert.ToByte(arg5 >> 8),
            Convert.ToByte(arg6 & 0xFF), Convert.ToByte(arg6 >> 8),
            Convert.ToByte(arg7 & 0xFF), Convert.ToByte(arg7 >> 8),
            Convert.ToByte(arg8 & 0xFF), Convert.ToByte(arg8 >> 8)
        };
        for (int i = 0; i < 16; i++)
        {
            databuf[wr_ptr][ptr] = args[i];
            ptr ++;
        }
        handleBufferFilled();
    }

    public static void addProcessData(ushort X, ushort Y, ushort Z, ushort A, ushort B)
    {
        addData(B, A, Z, Y, X, 0x00FF);
    }

    public static void addProcessJumpData(ushort X, ushort Y, ushort Z, ushort A, ushort B)
    {
        addData(B, A, Z, Y, X, 0);
    }

    public static void addProcessBegin()
    {
        handleBegin();
        addData(0, 0, 0, 0, 0, 0xFF00, 0, 0);
    }

    public static void addProcessEnd()
    {
        addData(0, 0, 0, 0, 0, 0x1100, 0, 0);
    }

    public static void setFreqData(int freq)
    {
        handleBegin();
        int cnt = 50000 / freq;
        addData(0xAA, 0, 0, 0, 0, 0xAA00, 0, 0);
        addData(Convert.ToUInt16(cnt  & 0xFFFF), Convert.ToUInt16(cnt >> 16));
        addData(0xAA, 0, 0, 0, 0, 0x5500, 0, 0);
    }

    public static void setPowerData(double power)
    {
        if(power > 100){
            power = 100;
        }
        power = 5.5366 + 2.67805 * power - 0.107836 * Math.Pow(power,2) + 0.00241519 * Math.Pow(power,3) - 0.0000248153 * Math.Pow(power,4) + 0.0000000964112 * Math.Pow(power,5);

        ushort p = Convert.ToUInt16(power * 65535 / 100);
        for (int i = 0; i < 10; i++)
        {
            addData(p, 0, 0, 0, 0, 0xbb00, 0, 11451);
        }
        forceFill();
        // addData(Convert.ToUInt16(p  & 0xFFFF), Convert.ToUInt16(p >> 16), 0, 0, 0, 0, 0, 11451);
    }

    public static void forceFill()
    {
        while(ptr < DATA_BUF_SIZE)
        {
            databuf[wr_ptr][ptr] = 0;
            ptr++;
        }
        handleBufferFilled();
    }

    public static void writeEnd(int p)
    {
        if (rd_queue.Count >= DATA_BUF_NUM)
        {
            Log.Warning("读队列异常");
        }
        rd_queue.Enqueue(p);
    }
    public static void readEnd(int p)
    {
        if(wr_queue.Count >= DATA_BUF_NUM)
        {
            Log.Warning("写队列异常");
        }
        wr_queue.Enqueue(p);
    }

    public static int getWriteBuf()
    {
        int p;
        while(!wr_queue.TryPeek(out p))
        {
            Thread.Sleep(10);
        }
        while(!wr_queue.TryDequeue(out p))
        {
            Log.Warning("弹出写队列失败");
        }
        return p;
    }

    public static int getReadBuf()
    {
        int p;
        while(!rd_queue.TryPeek(out p))
        {
            Thread.Sleep(10);
        }
        while(!rd_queue.TryDequeue(out p))
        {
            Log.Warning("弹出读队列失败");
        }
        return p;
    }

    private static void handleBufferFilled()
    {
        if (ptr >= DATA_BUF_SIZE)
        {
            if(! tcpThread.IsAlive)
            {
                Log.Information("启动tcp线程");
                tcpThread.Start();
            }
            Log.Information("写入成功，缓冲区: " + wr_ptr);
            writeEnd(wr_ptr);
            wr_ptr = getWriteBuf();
            ptr = 0;
        }
    }

    private static void handleBegin(){
        for (int i = 0; i < 2; i++)
        {
            addData(0, 0, 0, 0, 0, 0xFF00, 0, 0);
            DataBuffer.addProcessJumpData(0x8000,0x8000,0x8000,0x8000,0x8000);
            addData(0, 0, 0, 0, 0, 0x1100, 0, 0);
            forceFill();
        }
    }

}