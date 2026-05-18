using Grpc.Core;
using Serilog;
using laser_app_server_csharp.Processing;
namespace laser_app_server_csharp.Services;


public class FiveAxisService : FiveAxis.FiveAxisBase
{
    private readonly ILogger<FiveAxisService> _logger;
    public FiveAxisService(ILogger<FiveAxisService> logger)
    {
        _logger = logger;
    }
    public override Task<ServerReply> ProcessLine(LineData request, ServerCallContext context)
    {
        
        DataGenerator.genLineData(
            request.Speed,
            request.Times,
            request.X1,
            request.Y1,
            request.Z1,
            request.A1,
            request.B1,
            request.X2,
            request.Y2,
            request.Z2,
            request.A2,
            request.B2
        );
        if(request.IsLast)
        {
            DataBuffer.forceFill();
        }
        return Task.FromResult(new ServerReply
        {
            Code=0,
            Message="success"
        });
    }

    public override Task<ServerReply> ProcessCircle(CircleData request, ServerCallContext context)
    {
        Log.Information(Convert.ToString(request.CircleNumRepair));
        DataGenerator.genCircleData(
            request.Speed,
            request.Times,
            request.X1,
            request.Y1,
            request.X2,
            request.Y2,
            request.M,
            request.Angle,
            request.Taper,
            request.Filled,
            request.RMin,
            request.RInterval,
            request.ZStart,
            request.ZEnd,
            request.ZInterval,
            request.CircleNumRepair,
            request.TimesRepair  
        );
        if(request.IsLast)
        {
            DataBuffer.forceFill();
        }
        return Task.FromResult(new ServerReply
        {
            Code=0,
            Message="success"
        });
    }
    public override Task<ServerReply> ProcessRectangle(RectangleData request, ServerCallContext context)
    {
        DataGenerator.genFilledRectangleData(
            request.X0, request.Y0, request.X1, request.Y1, request.TaperAMax,
            request.TaperBMax, request.FeedSpacingX, request.FeedSpacingY, request.Speed, 0,
            request.ZStart, request.ZEnd, request.ZInterval, request.Times, request.X2, request.Y2,
            request.CircleNumRepair, request.TimesRepair
        );
        if(request.IsLast)
        {
            DataBuffer.forceFill();
        }
        return Task.FromResult(new ServerReply
        {
            Code=0,
            Message="success"
        });
    }

    public override Task<ServerReply> ProcessRectangle3D(Rectangle3DData request, ServerCallContext context)
    {
        DataBuffer.addProcessBegin();
        // DataGenerator.genFilledRectangleData3D_Y_and_Y2(
        //    0, 0, 3, 2.828, 4, 5.828, 8000, 0.1
        // );
        DataGenerator.genFilledRectangleData3D_Y_and_Y2(
            request.X0, request.Y0,request.Z0, request.X1, request.Y1, request.Z1, request.Speed, request.Interval, request.Times
        );
        DataBuffer.addProcessEnd();
        if(request.IsLast)
        {
            DataBuffer.forceFill();
        }
        
        return Task.FromResult(new ServerReply
        {
            Code=0,
            Message="success"
        });
    }

    public override Task<ServerReply> ProcessEllipse(EllipseData request, ServerCallContext context)
    {
        DataGenerator.genFilledEllipseData(
            request.Speed, request.Times, request.X0, request.Y0, request.AMax, request.BMax, request.AMin, request.BMin,
            request.FeedSpacingX, request.FeedSpacingY, request.TaperAMax, request.TaperBMax, request.ZStart, request.ZEnd,
            request.ZInterval, request.CircleNumRepair, request.TimesRepair
        );
        // Log.Information(Convert.ToString(request.ZStart));
        // Log.Information(Convert.ToString(request.ZEnd));
        // Log.Information(Convert.ToString(request.ZInterval));
        if(request.IsLast)
        {
            DataBuffer.forceFill();
        }

        return Task.FromResult(new ServerReply
        {
            Code=0,
            Message="success"
        });
    }
    public override Task<ServerReply> SetDelay(DelayData request, ServerCallContext context) 
    {
        DataGenerator.JUMP_SPEED = request.JUMPSPEED;
        DataGenerator.LASER_ON_DELAY = request.LASERONDELAY;
        DataGenerator.LASER_OFF_DELAY = request.LASEROFFDELAY;
        DataGenerator.MARK_DELAY = request.MARKDELAY;
        DataGenerator.JUMP_DELAY = request.JUMPDELAY;
        DataGenerator.POLYGON_DELAY = request.POLYGONDELAY;

        Log.Information(Convert.ToString(DataGenerator.JUMP_SPEED));

        return Task.FromResult(new ServerReply
        {
            Code=0,
            Message="success"
        });
    }
    public override Task<ServerReply> SetLaserFreq(FreqData request, ServerCallContext context)
    {
        DataGenerator.setFreq(request.Freq);
        return Task.FromResult(new ServerReply
        {
            Code=0,
            Message="success"
        });
    }

    public override Task<ServerReply> Test1(ServerReply request, ServerCallContext context)
    {
        DataGenerator.test1();
        DataBuffer.forceFill();
        return Task.FromResult(new ServerReply
        {
            Code=0,
            Message="success"
        });
    }

    public override Task<ServerReply> Test2(ServerReply request, ServerCallContext context)
    {
        DataGenerator.test2();
        DataBuffer.forceFill();
        return Task.FromResult(new ServerReply
        {
            Code=0,
            Message="success"
        });
    }
}