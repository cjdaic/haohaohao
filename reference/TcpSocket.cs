using System.Net;
using Serilog;
using System.Threading;
using System.Net.Sockets;
namespace laser_app_server_csharp.Processing;

public static class TcpSocket
{

    // private const string HOST = "127.0.0.1";
    // private const int PORT = 8888;

    private const string HOST = "192.168.1.10";
    private const int PORT = 7;

    public static void socketThread()
    {
        TcpClient tcpClient = new TcpClient();
        tcpClient.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.KeepAlive, true);
        tryConnect(ref tcpClient);
        while(true)
        {
            try
            {
                NetworkStream stream = tcpClient.GetStream();
                byte [] readData = new byte[128];
                stream.Read(readData, 0, 128);
                // Log.Information(readData.ToString());
                int rd_ptr = DataBuffer.getReadBuf();
                stream.Write(DataBuffer.databuf[rd_ptr], 0, DataBuffer.databuf[rd_ptr].Length);
                stream.Flush();
                
                DataBuffer.readEnd(rd_ptr);
            }
            catch (System.Exception)
            {
                tryConnect(ref tcpClient);
            }
            
        }
    }
    private static void tryConnect(ref TcpClient tcpClient)
    {
        while(! tcpClient.Connected)
        {
            try
            {
                tcpClient.Connect(HOST, PORT);
            }
            catch (System.Exception ex)
            {
                Log.Information(ex.ToString());
                Thread.Sleep(100);
            }         
        }
    }
}