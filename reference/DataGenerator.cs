
namespace laser_app_server_csharp.Processing;

using System.Globalization;
using Serilog;

public static class DataGenerator
{
    //Delay
    public static int JUMP_SPEED = 500;
    public static int LASER_ON_DELAY = 200;
    public static int LASER_OFF_DELAY = 25;
    public static int MARK_DELAY = 0;
    public static int JUMP_DELAY = 300;
    public static int POLYGON_DELAY = 0;


    //genDelayData
    public static void genDelayData(double X, double Y, double Z,
    double A, double B, int delay, int delayOn)
    {
        int t = 10;
        int n = delay / t;
        int i = 0;
        correctionly(ref X, ref Y, ref Z);
        while (i < delayOn / t)
        {
            DataBuffer.addProcessData(Convert.ToUInt16(X),
                            Convert.ToUInt16(Y), Convert.ToUInt16(Z),
                            Convert.ToUInt16(A), Convert.ToUInt16(B));
            i = i + 1;
        }

        while (i < n)
        {
            try{
                DataBuffer.addProcessJumpData(Convert.ToUInt16(X),
                            Convert.ToUInt16(Y), Convert.ToUInt16(Z),
                            Convert.ToUInt16(A), Convert.ToUInt16(B));
            }
            catch{
                Console.WriteLine(Z);
            }
            
            i = i + 1;
        }

    }

    //genLineDataNew
    public static void genLineDataNew(double speed, int Laser_Switch, double X1,
        double Y1, double Z1, double A1, double B1, double X2, double Y2,
        double Z2, double A2, double B2)
    {
        speed *= 0.001;
        double length_xy = 0.001 * Math.Sqrt(Math.Pow(X2 - X1, 2) + Math.Pow(Y2 - Y1, 2));
        double length_z = Math.Abs(0.001 * (Z2 - Z1));
        double length_ab = 0.001 * Math.Sqrt(Math.Pow(A2 - A1, 2) + Math.Pow(B2 - B1, 2));
        double t = 0.00001; // 10 us

        int n_max = Convert.ToInt32(length_xy / (speed * t));
        if (length_xy != 0)
        {
            n_max = Convert.ToInt32((length_xy / (speed * t)) + 1);
        }
        else if (length_z != 0)
        {
            n_max = Convert.ToInt32((length_z / (speed * t)) + 1);
            length_xy = length_z;
        }
        else
        {
            n_max = Convert.ToInt32((length_ab / (speed * t)) + 1);
            length_xy = length_ab;
        }

        if (X1 == X2 && Y1 == Y2 && Z1 == Z2 && A1 == A2 && B1 == B2)
        {
            correction(ref X1, ref Y1, ref Z1, ref A1, ref B1);
            DataBuffer.addProcessJumpData(Convert.ToUInt16(X1),
                            Convert.ToUInt16(Y1), Convert.ToUInt16(Z1),
                            Convert.ToUInt16(A1), Convert.ToUInt16(B1));
        }
        else
        {
            for (int i = 1; i <= n_max; i++)
            {
                double X = X1 + i * (X2 - X1) * speed * t / length_xy;
                double Y = Y1 + i * (Y2 - Y1) * speed * t / length_xy;
                double Z = Z1 + i * (Z2 - Z1) * speed * t / length_xy;
                double A = A1 + i * (A2 - A1) * speed * t / length_xy;
                double B = B1 + i * (B2 - B1) * speed * t / length_xy;

                correction(ref X, ref Y, ref Z, ref A, ref B);

                if (Laser_Switch == 1)
                {
                    if (i * 10 < LASER_ON_DELAY)
                    {
                        DataBuffer.addProcessJumpData(Convert.ToUInt16(X),
                                Convert.ToUInt16(Y), Convert.ToUInt16(Z),
                                Convert.ToUInt16(A), Convert.ToUInt16(B));
                    }
                    else
                    {
                        DataBuffer.addProcessData(Convert.ToUInt16(X),
                                Convert.ToUInt16(Y), Convert.ToUInt16(Z),
                                Convert.ToUInt16(A), Convert.ToUInt16(B));
                    }
                }
                else
                {
                    DataBuffer.addProcessJumpData(Convert.ToUInt16(X),
                                 Convert.ToUInt16(Y), Convert.ToUInt16(Z),
                                 Convert.ToUInt16(A), Convert.ToUInt16(B));
                }
            }
        }

    }

    //genLineDataNoDelay
    public static void genLineDataNoDelay(double speed, int Laser_Switch, double X1,
        double Y1, double Z1, double A1, double B1, double X2, double Y2,
        double Z2, double A2, double B2)
    {
        speed *= 0.001;
        double length_xy = 0.001 * Math.Sqrt(Math.Pow(X2 - X1, 2) + Math.Pow(Y2 - Y1, 2));
        double length_z = Math.Abs(0.001 * (Z2 - Z1));
        double t = 0.00001; // 10 us

        int n_max = Convert.ToInt32(length_xy / (speed * t));
        if (length_xy != 0)
        {
            n_max = Convert.ToInt32((length_xy / (speed * t)) + 1);
        }
        else
        {
            n_max = Convert.ToInt32((length_z / (speed * t)) + 1);
            length_xy = length_z;
        }
        if (X1 == X2 && Y1 == Y2 && Z1 == Z2 && A1 == A2 && B1 == B2)
        {
            correction(ref X1, ref Y1, ref Z1, ref A1, ref B1);
            DataBuffer.addProcessJumpData(Convert.ToUInt16(X1),
                            Convert.ToUInt16(Y1), Convert.ToUInt16(Z1),
                            Convert.ToUInt16(A1), Convert.ToUInt16(B1));
        }

        for (int i = 1; i <= n_max; i++)
        {
            double X = X1 + i * (X2 - X1) * speed * t / length_xy;
            double Y = Y1 + i * (Y2 - Y1) * speed * t / length_xy;
            double Z = Z1 + i * (Z2 - Z1) * speed * t / length_xy;
            double A = A1 + i * (A2 - A1) * speed * t / length_xy;
            double B = B1 + i * (B2 - B1) * speed * t / length_xy;
            correction(ref X, ref Y, ref Z, ref A, ref B);
            DataBuffer.addProcessData(Convert.ToUInt16(X),
                            Convert.ToUInt16(Y), Convert.ToUInt16(Z),
                            Convert.ToUInt16(A), Convert.ToUInt16(B));
        }
    }

    //genCircleDataNew
    public static void genCircleNew(double X0, double Y0, double X1, double Y1,
        double Z1, double Laser_Taper, double speed)
    {

        int Laser_direction_1;

        speed *= 0.001;

        if (Laser_Taper > 0)
        {
            Laser_direction_1 = 0;
        }
        else
        {
            Laser_direction_1 = 1;
        }

        Laser_Taper = Math.Abs(Laser_Taper);

        double R;//计算圆半径
        R = Math.Sqrt((X1 - X0) * (X1 - X0) + (Y1 - Y0) * (Y1 - Y0));
        R = 0.001 * R;

        double t;//数据间隔时间10μs
        t = 0.00001;

        double T;
        T = 2 * Math.PI * R / speed;

        int n_max = Convert.ToInt32((T / t) + 1);
        int n = 0;

        while (n < n_max)
        {
            double X;
            double Y;
            double A;
            double B;
            double Z = Z1;

            X = X0 + R * Math.Cos((n * t * 360 / T) * 3.14 / 180) * 1000;//第n个数据点X坐标
            Y = Y0 + R * Math.Sin((n * t * 360 / T) * 3.14 / 180) * 1000;//第n个数据点Y坐标
            A = Laser_Taper * Math.Sin(Laser_direction_1 * 3.14 + (n * t * 360 / T) * 3.14 / 180);
            B = Laser_Taper * Math.Cos(Laser_direction_1 * 3.14 + (n * t * 360 / T) * 3.14 / 180);

            correction(ref X, ref Y, ref Z, ref A, ref B);
            if (n * 10 < LASER_ON_DELAY)
            {

                DataBuffer.addProcessJumpData(Convert.ToUInt16(X),
                                Convert.ToUInt16(Y), Convert.ToUInt16(Z),
                                Convert.ToUInt16(A), Convert.ToUInt16(B));
            }
            else
            {
                DataBuffer.addProcessData(Convert.ToUInt16(X),
                                Convert.ToUInt16(Y), Convert.ToUInt16(Z),
                                Convert.ToUInt16(A), Convert.ToUInt16(B));
            }
            n = n + 1;
        }
    }

    //genRectangleData
    public static void genRectangleData(double X0, double Y0, double X1, double Y1,
        double Z1, double Laser_Taper_X, double Laser_Taper_Y, double V)
    {
        double X_length = Math.Abs(2 * (X1 - X0));
        double Y_length = Math.Abs(2 * (Y1 - Y0));
        double X_Start = X1;
        double Y_Start = Y1;
        double Z_Start = Z1;
        double A_Start = Laser_Taper_Y;
        double B_Start = Laser_Taper_X;
        double V1 = V;

        genLineDataNew(V1, 1, X_Start, Y_Start, Z_Start, A_Start, B_Start, X_Start - X_length, Y_Start, Z_Start, A_Start, -B_Start);
        genDelayData(X_Start - X_length, Y_Start, Z_Start, A_Start, -B_Start, POLYGON_DELAY, POLYGON_DELAY);
        genLineDataNoDelay(V1, 1, X_Start - X_length, Y_Start, Z_Start, A_Start, -B_Start, X_Start - X_length, Y_Start - Y_length, Z_Start, -A_Start, -B_Start);
        genDelayData(X_Start - X_length, Y_Start - Y_length, Z_Start, -A_Start, -B_Start, POLYGON_DELAY, POLYGON_DELAY);
        genLineDataNoDelay(V1, 1, X_Start - X_length, Y_Start - Y_length, Z_Start, -A_Start, -B_Start, X_Start, Y_Start - Y_length, Z_Start, -A_Start, B_Start);
        genDelayData(X_Start, Y_Start - Y_length, Z_Start, -A_Start, B_Start, POLYGON_DELAY, POLYGON_DELAY);
        genLineDataNoDelay(V1, 1, X_Start, Y_Start - Y_length, Z_Start, -A_Start, B_Start, X_Start, Y_Start, Z_Start, A_Start, B_Start);
        genDelayData(X_Start, Y_Start, Z_Start, A_Start, B_Start, MARK_DELAY, LASER_OFF_DELAY);
    }


    //genEllipseData
    public static void genEllipseData(double X0, double Y0, double a, double b,
        double Z, double Laser_Taper_X, double Laser_Taper_Y, double V)
    {

        double l = 2 * 3.1415 * b + 4 * (a - b);//椭圆周长
        double t = 0.00001;
        int T = Convert.ToInt32(l / (V * t));//标刻总点数
        double T_calculate = T * 10;//分割总点数
        double AngleSpacing_calculate = 360 / T_calculate;//分割角度间隔
        double l_min = V * t;//相邻两点间的长度
        double Laser_direction_1;
        double Laser_direction_2;

        if (Laser_Taper_X > 0)
        {
            Laser_direction_1 = 0;
        }
        else
        {
            Laser_direction_1 = 1;
        }

        if (Laser_Taper_Y > 0)
        {
            Laser_direction_2 = 0;
        }
        else
        {
            Laser_direction_2 = 1;
        }

        double distance_total = 0;
        int PointNumReal = 0;
        int i = 0;
        while (i < T_calculate)
        {
            double angle1 = i * AngleSpacing_calculate;//角度
            double x1 = X0 + a * Math.Cos(angle1 * 3.14 / 180);
            double y1 = Y0 + b * Math.Sin(angle1 * 3.14 / 180);


            for (int j = 0; j < 50; j++)
            {
                double angle2 = (i + j) * AngleSpacing_calculate;//角度
                double x2 = X0 + a * Math.Cos(angle2 * 3.14 / 180);
                double y2 = Y0 + b * Math.Sin(angle2 * 3.14 / 180);
                double distance = Math.Sqrt(Math.Pow((x2 - x1), 2) + Math.Pow((y2 - y1), 2));
                distance_total = distance_total + distance;

                if (distance_total >= l_min)
                {
                    PointNumReal += 1;
                    distance_total = 0;
                    double X = x2;
                    double Y = y2;
                    double Z1 = Z;
                    double A = Math.Abs(Laser_Taper_Y) * Math.Sin(Laser_direction_2 * 3.14 + (angle2 * 3.14 / 180));
                    double B = Math.Abs(Laser_Taper_X) * Math.Cos(Laser_direction_1 * 3.14 + (angle2 * 3.14 / 180));

                    correction(ref X, ref Y, ref Z1, ref A, ref B);
                    if (PointNumReal * 10 < LASER_ON_DELAY)
                    {

                        DataBuffer.addProcessJumpData(Convert.ToUInt16(X),
                            Convert.ToUInt16(Y), Convert.ToUInt16(Z1),
                            Convert.ToUInt16(A), Convert.ToUInt16(B));
                    }
                    else
                    {
                        DataBuffer.addProcessData(Convert.ToUInt16(X),
                            Convert.ToUInt16(Y), Convert.ToUInt16(Z1),
                            Convert.ToUInt16(A), Convert.ToUInt16(B));
                    }
                    i = i + j;
                    break;
                }
            }
        }
    }

    //genFilledRectangleData
    public static void genFilledRectangleData(double X0, double Y0, double X1, double Y1, double taper_A_Max,
        double taper_B_Max, double FeedSpacing_X, double FeedSpacing_Y, double ScanSpeed, double _,
        double z_start, double z_end, double z_interval, int times, double X2, double Y2, int circle_num_repair, int times_repair)
    {

        int Num_of_rectangles_x = Convert.ToInt32(Math.Abs(X1 - X2) / FeedSpacing_X) + 1;//x方向扫描矩形数
        int Num_of_rectangles_y = Convert.ToInt32(Math.Abs(Y1 - Y2) / FeedSpacing_Y) + 1;//y方向扫描矩形数
        int Num_of_rectangles = Num_of_rectangles_x < Num_of_rectangles_y ? Num_of_rectangles_x : Num_of_rectangles_y;//扫描矩形数
        double X_Min_Real = X1 - (Num_of_rectangles - 1) * FeedSpacing_X;//实际最内矩形起点X坐标
        double Y_Min_Real = Y1 - (Num_of_rectangles - 1) * FeedSpacing_Y;//实际最内矩形起点Y坐标
        double B_Min_Real = taper_B_Max / Num_of_rectangles_x;//实际最内矩形B轴
        double A_Min_Real = taper_A_Max / Num_of_rectangles_y;//实际最内矩形A轴
        //Z轴进给计算
        int h = 0;
        if ((z_end - z_start) > 0)//Z轴运动方向判断
        {
            h = 1;
        }
        else
        {
            h = -1;
        }
        //Z轴扫描总次数
        int Z_Scantimes = 0;
        if (z_interval == 0)
        {
            Z_Scantimes = 1;
        }
        else
        {
            Z_Scantimes = Convert.ToInt32(Math.Abs(z_end - z_start) / z_interval + 1);
        }
        DataBuffer.addProcessBegin();
        double Z = z_start;//定义初始Z轴坐标
        genLineDataNew(JUMP_SPEED, 0, 0, 0, 0, 0, 0, 0, 0, Z, 0, 0);
        genDelayData(0, 0, Z, 0, 0, JUMP_DELAY, 0);

        for (int k = 0; k < Z_Scantimes; k++)//Z轴进给循环
        {
            for (int j = 0; j < times; j++)//一个Z轴位置处的扫描循环
            {
                double X = X_Min_Real;
                double Y = Y_Min_Real;
                double A = A_Min_Real;
                double B = B_Min_Real;

                //Jump从原点跳到第一个方的起点
                genLineDataNew(JUMP_SPEED, 0, 0, 0, z_start + k * h * z_interval, 0, 0, X, Y, Z, A, B);
                //JumpDelay
                genDelayData(X, Y, Z, A, B, JUMP_DELAY, 0);

                //主循环-由内到外
                int i = 0;
                while (i < Num_of_rectangles)//主循环-由内到外
                {
                    X = X_Min_Real + i * FeedSpacing_X;
                    Y = Y_Min_Real + i * FeedSpacing_Y;
                    A = A_Min_Real + i * (taper_A_Max / Num_of_rectangles);
                    B = B_Min_Real + i * (taper_B_Max / Num_of_rectangles);

                    //MarkRectangle
                    genRectangleData(X0, Y0, X, Y, Z, B, A, ScanSpeed);//B轴为X轴的锥度；A轴为Y轴的锥度

                    i = i + 1;

                    double X_Temp = X;
                    double Y_Temp = Y;
                    double A_Temp = A;
                    double B_Temp = B;

                    X = X + FeedSpacing_X;
                    Y = Y + FeedSpacing_Y;
                    A = A + taper_A_Max / Num_of_rectangles;
                    B = B + taper_B_Max / Num_of_rectangles;

                    if (i == Num_of_rectangles)//如果所有循环扫描完成，则跳转回原点，并退出循环
                    {
                        //Jump
                        genLineDataNew(JUMP_SPEED, 0, X_Temp, Y_Temp, Z, A_Temp, B_Temp, X0, Y0, Z, 0, 0);
                        break;
                    }
                    else//否则跳转到下一个矩形的起点

                        //Jump
                        genLineDataNew(JUMP_SPEED, 0, X_Temp, Y_Temp, Z, A_Temp, B_Temp, X, Y, Z, A, B);
                    //JumpDelay
                    genDelayData(X, Y, Z, A, B, JUMP_DELAY, 0);
                }

                for (int l = 0; l < times_repair; l++)
                {

                    X = X1 - (circle_num_repair - 1) * FeedSpacing_X;
                    Y = Y1 - (circle_num_repair - 1) * FeedSpacing_Y;
                    A = (Num_of_rectangles - circle_num_repair) * taper_A_Max / Num_of_rectangles;
                    B = (Num_of_rectangles - circle_num_repair) * taper_B_Max / Num_of_rectangles;
                    //Jump从原点跳到修边最内圈起点
                    genLineDataNew(JUMP_SPEED, 0, X0, Y0, Z, 0, 0, X, Y, Z, A, B);
                    //JumpDelay
                    genDelayData(X, Y, Z, A, B, JUMP_DELAY, 0);

                    for (int m = 0; m < circle_num_repair; m++)
                    {
                        X = X1 - (circle_num_repair - 1) * FeedSpacing_X + m * FeedSpacing_X;
                        Y = Y1 - (circle_num_repair - 1) * FeedSpacing_Y + m * FeedSpacing_Y;
                        A = (Num_of_rectangles - circle_num_repair) * taper_A_Max / Num_of_rectangles + m * taper_A_Max / Num_of_rectangles;
                        B = (Num_of_rectangles - circle_num_repair) * taper_B_Max / Num_of_rectangles + m * taper_B_Max / Num_of_rectangles;

                        //MarkRectangle
                        genRectangleData(X0, Y0, X, Y, Z, B, A, ScanSpeed);//B轴为X轴的锥度；A轴为Y轴的锥度
                        //MarkDelay
                        genDelayData(X, Y, Z, A, B, MARK_DELAY, LASER_OFF_DELAY);

                        double X_Temp = X;
                        double Y_Temp = Y;
                        double A_Temp = A;
                        double B_Temp = B;

                        X = X + FeedSpacing_X;
                        Y = Y + FeedSpacing_Y;
                        A = A + taper_A_Max / Num_of_rectangles;
                        B = B + taper_B_Max / Num_of_rectangles;

                        if (m == circle_num_repair - 1)//如果所有循环扫描完成，则跳转回原点，并退出循环
                        {
                            //Jump
                            genLineDataNew(JUMP_SPEED, 0, X_Temp, Y_Temp, Z, A_Temp, B_Temp, X0, Y0, Z, 0, 0);
                            break;
                        }
                        else//否则跳转到下一个矩形的起点

                            //Jump
                            genLineDataNew(JUMP_SPEED, 0, X_Temp, Y_Temp, Z, A_Temp, B_Temp, X, Y, Z, A, B);
                        //JumpDelay
                        genDelayData(X, Y, Z, A, B, JUMP_DELAY, 0);
                    }
                }
                Z = z_start + (k + 1) * h * z_interval;
            }
        }
        //Jump
        genLineDataNew(JUMP_SPEED, 0, 0, 0, Z, 0, 0, 0, 0, 0, 0, 0);
        DataBuffer.addProcessEnd();
    }

    public static void genLineData(double speed, int times, double X1,
        double Y1, double Z1, double A1, double B1, double X2, double Y2,
        double Z2, double A2, double B2)
    {

        speed *= 0.001;
        double length_xy = 0.001 * Math.Sqrt(Math.Pow(X2 - X1, 2) + Math.Pow(Y2 - Y1, 2));
        double t = 0.00001; // 10 us
        DataBuffer.addProcessBegin();
        int n_max = Convert.ToInt32(length_xy / (speed * t));
        for (int i = 0; i < n_max; i++)
        {
            double X = X1 + i * (X2 - X1) * speed * t / length_xy;
            double Y = Y1 + i * (Y2 - Y1) * speed * t / length_xy;
            double Z = Z1 + i * (Z2 - Z1) * speed * t / length_xy;
            double A = A1 + i * (A2 - A1) * speed * t / length_xy;
            double B = B1 + i * (B2 - B1) * speed * t / length_xy;
            correction(ref X, ref Y, ref Z, ref A, ref B);
            DataBuffer.addProcessData(Convert.ToUInt16(X),
                Convert.ToUInt16(Y), Convert.ToUInt16(Z),
                Convert.ToUInt16(A), Convert.ToUInt16(B));
        }
        DataBuffer.addProcessEnd();
    }

    //填充同心圆
    public static void genCircleData(double speed, int times, double X1,
        double Y1, double X2, double Y2, int M, double angle, double taper,
        bool filled, double r_min, double r_interval, double z_start,
        double z_end, double z_interval, int circle_num_repair, int times_repair)
    {
        //数据生成准备
        double r_max = Math.Sqrt(Math.Pow(X2 - X1, 2) + Math.Pow(Y2 - Y1, 2));
        int circle_num = Convert.ToInt32((r_max - r_min) / r_interval + 1);
        double r_min_real = r_max - (circle_num - 1) * r_interval;
        double L_I;//子循环激光锥度
        L_I = taper / circle_num;//初始扫描最内圈激光锥度

        //Z轴进给计算
        int h = 0;
        if ((z_end - z_start) > 0)//Z轴运动方向判断
        {
            h = 1;
        }
        else
        {
            h = -1;
        }

        //Z轴扫描总次数(Z轴进给次数)

        int Z_Scantimes = 0;

        if (z_interval == 0)
        {
            Z_Scantimes = 1;
        }
        else
        {
            Z_Scantimes = Convert.ToInt32(Math.Abs(z_end - z_start) / z_interval + 1);
        }

        DataBuffer.addProcessBegin();

        double Z = z_start;//定义初始Z轴坐标

        genLineDataNew(JUMP_SPEED, 0, 0, 0, 0, 0, 0, 0, 0, Z, 0, 0);

        for (int k = 0; k < Z_Scantimes; k++)//Z轴进给循环
        {
            for (int j = 0; j < times; j++)//一个Z轴位置处的扫描循环
            {
                double X = X1 + r_min_real;
                double Y = Y1;
                double A = 0;
                double B = taper / circle_num;

                //Jump从原点跳到第一个圆的起点
                genLineDataNew(JUMP_SPEED, 0, 0, 0, z_start + k * h * z_interval, 0, 0, X, Y, Z, A, B);
                //JumpDelay
                genDelayData(X, Y, Z, A, B, JUMP_DELAY, 0);

                //主循环-由内到外
                int i = 0;
                while (i < circle_num)//主循环-由内到外
                {

                    L_I = (i + 1) * taper / circle_num;//子循环锥度
                    B = L_I;

                    //MarkCircle
                    genCircleNew(X1, Y1, X, Y, Z, L_I, speed);
                    //MarkDelay
                    genDelayData(X, Y, Z, A, B, MARK_DELAY, LASER_OFF_DELAY);
                    i = i + 1;

                    double X_Temp = X;
                    double B_Temp = B;

                    X = X + r_interval;//子循环X坐标变化
                    B = (i + 1) * taper / circle_num;

                    if (i == circle_num)//如果所有循环扫描完成，则跳转回原点，并退出循环
                    {
                        //Jump
                        genLineDataNew(JUMP_SPEED, 0, X_Temp, Y, Z, A, B_Temp, X1, Y1, Z, 0, 0);
                        break;
                    }
                    else//否则跳转到下一个圆的起点
                        //Jump
                        genLineDataNew(JUMP_SPEED, 0, X_Temp, Y, Z, A, B_Temp, X, Y, Z, A, B);
                    //JumpDelay
                    genDelayData(X, Y, Z, A, B, JUMP_DELAY, 0);
                }
                //修边
                for (int l = 0; l < times_repair; l++)
                {

                    X = X2 - (circle_num_repair - 1) * r_interval;
                    Y = Y1;
                    B = (circle_num - circle_num_repair) * taper / circle_num;
                    //Jump从原点跳到修边最内圈起点
                    genLineDataNew(JUMP_SPEED, 0, X1, Y1, Z, 0, 0, X, Y, Z, A, B);
                    //JumpDelay
                    genDelayData(X, Y, Z, A, B, JUMP_DELAY, 0);

                    for (int m = 0; m < circle_num_repair; m++)
                    {

                        L_I = (circle_num - circle_num_repair) * taper / circle_num + m * taper / circle_num;//子循环锥度
                        B = L_I;

                        //MarkCircle
                        genCircleNew(X1, Y1, X, Y, Z, L_I, speed);
                        //MarkDelay
                        genDelayData(X, Y, Z, A, B, MARK_DELAY, LASER_OFF_DELAY);

                        double X_Temp = X;
                        double B_Temp = B;

                        X = X + r_interval;//子循环X坐标变化
                        B = (circle_num - circle_num_repair) * taper / circle_num + (m + 1) * taper / circle_num;

                        if (m == circle_num_repair - 1)//如果所有循环扫描完成，则跳转回原点，并退出循环
                        {
                            //Jump
                            genLineDataNew(JUMP_SPEED, 0, X_Temp, Y, Z, A, B_Temp, X1, Y1, Z, 0, 0);
                            break;
                        }
                        else//否则跳转到下一个圆的起点
                            //Jump
                            genLineDataNew(JUMP_SPEED, 0, X_Temp, Y, Z, A, B_Temp, X, Y, Z, A, B);
                        //JumpDelay
                        genDelayData(X, Y, Z, A, B, JUMP_DELAY, 0);
                    }
                }
                Z = z_start + (k + 1) * h * z_interval;
            }
        }
        //Jump
        genLineDataNew(JUMP_SPEED, 0, 0, 0, Z, 0, 0, 0, 0, 0, 0, 0);
        DataBuffer.addProcessEnd();
    }


    //填充同心椭圆
    public static void genFilledEllipseData(double speed, int times, double X0, double Y0, double a_Max, double b_Max, double a_Min, double b_Min,
        double FeedSpacing_X, double FeedSpacing_Y, double taper_A_Max, double taper_B_Max, double z_start, double z_end, double z_interval,
        int circle_num_repair, int times_repair)
    {

        //数据生成准备
        int Num_of_ellipses_x = Convert.ToInt32(Math.Abs(a_Max - a_Min) / FeedSpacing_X) + 1;//x方向扫描椭圆数
        int Num_of_ellipses_y = Convert.ToInt32(Math.Abs(b_Max - b_Min) / FeedSpacing_Y) + 1;//y方向扫描椭圆数
        int Num_of_ellipses = Num_of_ellipses_x < Num_of_ellipses_y ? Num_of_ellipses_x : Num_of_ellipses_y;//扫描矩形数
        double a_Min_Real = a_Max - (Num_of_ellipses - 1) * FeedSpacing_X;//实际最内椭圆起点X坐标
        double b_Min_Real = b_Max - (Num_of_ellipses - 1) * FeedSpacing_Y;//实际最内椭圆起点X坐标
        double taper_Y_Min_Real = taper_B_Max / Num_of_ellipses;//实际最内椭圆Y轴最小锥度
        double taper_X_Min_Real = taper_A_Max / Num_of_ellipses;//实际最内椭圆X轴最小锥度

        //Z轴进给计算
        int h = 0;
        if ((z_end - z_start) > 0)//Z轴运动方向判断
        {
            h = 1;
        }
        else
        {
            h = -1;
        }

        //Z轴扫描总次数(Z轴进给次数)

        int Z_Scantimes = 0;

        if (z_interval == 0)
        {
            Z_Scantimes = 1;
        }
        else
        {
            Z_Scantimes = Convert.ToInt32(Math.Abs(z_end - z_start) / z_interval + 1);
        }

        DataBuffer.addProcessBegin();

        double Z = z_start;//定义初始Z轴坐标

        genLineDataNew(JUMP_SPEED, 0, 0, 0, 0, 0, 0, 0, 0, Z, 0, 0);

        for (int k = 0; k < Z_Scantimes; k++)//Z轴进给循环
        {
            for (int j = 0; j < times; j++)//一个Z轴位置处的扫描循环
            {
                double a = a_Min_Real;
                double b = b_Min_Real;
                double A = 0;
                double B = taper_X_Min_Real;
                double X = X0 + a_Min_Real;
                double Y = Y0;
                //Jump从原点跳到第一个椭圆的起点
                genLineDataNew(JUMP_SPEED, 0, 0, 0, z_start + k * h * z_interval, 0, 0, X, Y, Z, A, B);
                //JumpDelay
                genDelayData(X, Y, Z, A, B, JUMP_DELAY, 0);

                //主循环-由内到外
                int i = 0;
                while (i < Num_of_ellipses)//主循环-由内到外
                {
                    double taper_A_Max_1 = (i + 1) * taper_A_Max / Num_of_ellipses;
                    double taper_B_Max_1 = (i + 1) * taper_B_Max / Num_of_ellipses;
                    a = a_Min_Real + i * FeedSpacing_X;
                    b = b_Min_Real + i * FeedSpacing_Y;

                    //MarkEllipse
                    genEllipseData(X0, Y0, a, b, Z, taper_A_Max_1, taper_B_Max_1, speed);

                    B = (i + 1) * taper_A_Max / Num_of_ellipses;
                    X = X0 + a_Min_Real + i * FeedSpacing_X;

                    //MarkDelay
                    genDelayData(X, Y, Z, A, B, MARK_DELAY, LASER_OFF_DELAY);

                    double X_Temp = X;
                    double B_Temp = B;

                    i = i + 1;

                    X = X + FeedSpacing_X;//子循环X坐标变化
                    B = (i + 1) * taper_A_Max / Num_of_ellipses;//子循环B坐标变化

                    if (i == Num_of_ellipses)//如果所有循环扫描完成，则跳转回原点，并退出循环
                    {
                        //Jump
                        genLineDataNew(JUMP_SPEED, 0, X_Temp, Y, Z, A, B_Temp, X0, Y0, Z, 0, 0);
                        break;
                    }
                    else//否则跳转到下一个圆的起点
                        //Jump
                        genLineDataNew(JUMP_SPEED, 0, X_Temp, Y, Z, A, B_Temp, X, Y, Z, A, B);
                    //JumpDelay
                    genDelayData(X, Y, Z, A, B, JUMP_DELAY, 0);
                }
                //修边
                for (int l = 0; l < times_repair; l++)
                {
                    X = X0 + a_Min_Real + (Num_of_ellipses - circle_num_repair) * FeedSpacing_X;
                    B = (Num_of_ellipses - circle_num_repair) * taper_A_Max / Num_of_ellipses;

                    //Jump从原点跳到修边最内圈起点
                    genLineDataNew(JUMP_SPEED, 0, X0, Y0, Z, 0, 0, X, Y, Z, A, B);
                    //JumpDelay
                    genDelayData(X, Y, Z, A, B, JUMP_DELAY, 0);

                    for (int m = 0; m < circle_num_repair; m++)
                    {
                        double taper_A_Max_1 = (Num_of_ellipses - circle_num_repair + m) * taper_A_Max / Num_of_ellipses;
                        double taper_B_Max_1 = (Num_of_ellipses - circle_num_repair + m) * taper_B_Max / Num_of_ellipses;
                        B = (Num_of_ellipses - circle_num_repair) * taper_A_Max / Num_of_ellipses + m * taper_A_Max / Num_of_ellipses;
                        X = X0 + a_Min_Real + (Num_of_ellipses - circle_num_repair + m) * FeedSpacing_X;
                        a = a_Min_Real + (Num_of_ellipses - circle_num_repair + m) * FeedSpacing_X;
                        b = b_Min_Real + (Num_of_ellipses - circle_num_repair + m) * FeedSpacing_Y;
                        //MarkEllipse
                        genEllipseData(X0, Y0, a, b, Z, taper_A_Max_1, taper_B_Max_1, speed);
                        //MarkDelay
                        genDelayData(X, Y, Z, A, B, MARK_DELAY, LASER_OFF_DELAY);

                        double X_Temp = X;
                        double B_Temp = B;

                        X = X + FeedSpacing_X;//子循环X坐标变化
                        B = (Num_of_ellipses - circle_num_repair) * taper_A_Max / Num_of_ellipses + (m + 1) * taper_A_Max / Num_of_ellipses;

                        if (m == circle_num_repair - 1)//如果所有循环扫描完成，则跳转回原点，并退出循环
                        {
                            //Jump
                            genLineDataNew(JUMP_SPEED, 0, X_Temp, Y, Z, A, B_Temp, X0, Y0, Z, 0, 0);
                            break;
                        }
                        else//否则跳转到下一个圆的起点
                            //Jump
                            genLineDataNew(JUMP_SPEED, 0, X_Temp, Y, Z, A, B_Temp, X, Y, Z, A, B);
                        //JumpDelay
                        genDelayData(X, Y, Z, A, B, JUMP_DELAY, 0);
                    }
                }
                Z = z_start + (k + 1) * h * z_interval;

            }
        }
        //Jump

        genLineDataNew(JUMP_SPEED, 0, 0, 0, Z, 0, 0, 0, 0, 0, 0, 0);
        DataBuffer.addProcessEnd();
    }



    public static void setFreq(int freq)
    {
        DataBuffer.setFreqData(freq);
        DataBuffer.forceFill();
    }

    public static void correction(ref double X, ref double Y, ref double Z,
        ref double A, ref double B, double zz = 10500)
    {

        // 增益
        double X_x = 780.488;
        double Y_y = 788.177;
        double Z_z = -zz;
        double A_a = 1024.0;
        double B_b = 1024.0;

        // Z轴斜矫正
        //X = X - (X / 20.0) * 0.063 * Z;
        //Y = Y + (Y / 20.0) * 0.005 * Z;

        //边缘椭圆矫正
        //int m;
        //if (Y > 0)
        //{
            //m = 1;
        //}
        //else
        //{
           // m = -1;
        //}
        //Y = Y + m * (Math.Abs(A) / 3.5) * 0.0075 * (Math.Abs(X) / 17.25);
        //X = Z;



        X = X * X_x;
        Y = Y * Y_y;
        Z = (-0.11 + 1.075 * Z) * Z_z;
        A = A * A_a;
        B = B * B_b;


        double degree = 45;
        double tmp_radian = degree * Math.PI / 180;
        double temp_X = X;
        double temp_Y = Y;        

        X = temp_X * Math.Cos(tmp_radian) + temp_Y * Math.Sin(tmp_radian);
        Y = temp_Y * Math.Cos(tmp_radian) - temp_X * Math.Sin(tmp_radian);


        X += 32768;
        Y += 32768;
        Z += 32768;
        A += 32768;
        B += 32768;
    }


    //后端




    //二维平面等距螺旋线-速度均匀-锥度一致（锥度大时速度有限制）x
    public static void gen2DSpialLineDataNew(double speed, double times, double a, double b, double Laser_Taper,
    double R_Max, double X0, double Y0, double z_start, double z_end, double z_interval)
    {

        //计算参数
        double Laser_direction_1;
        double theta_max = (R_Max - a) / b;
        double t = 0.00001;
        double circle_total = theta_max / (2 * Math.PI);//总扫描圈数

        if (Laser_Taper > 0)
        {
            Laser_direction_1 = 0;
        }
        else
        {
            Laser_direction_1 = 1;
        }

        Laser_Taper = Math.Abs(Laser_Taper);

        //Z轴进给计算
        int h = 0;
        if ((z_end - z_start) > 0)//Z轴运动方向判断
        {
            h = 1;
        }
        else
        {
            h = -1;
        }

        //Z轴扫描总次数(Z轴进给次数)

        int Z_Scantimes = 0;

        if (z_interval == 0)
        {
            Z_Scantimes = 1;
        }
        else
        {
            Z_Scantimes = Convert.ToInt32(Math.Abs(z_end - z_start) / z_interval + 1);

        }

        double Z = z_start;//定义初始Z轴坐标

        DataBuffer.addProcessBegin();

        genLineDataNew(JUMP_SPEED, 0, 0, 0, 0, 0, 0, 0, 0, Z, 0, 0);

        for (int k = 0; k < Z_Scantimes; k++)//Z轴进给循环
        {
            for (int l = 0; l < times; l++)//一个Z轴位置处的扫描循环
            {
                double X_start = X0 + a;
                double Y_start = Y0;
                double A_start = 0;
                double B_start = Laser_Taper * Math.Cos(Laser_direction_1 * 3.14);
                //Jump从原点跳到第一个椭圆的起点,跳转速度设置为0.1，B轴行程太大
                genLineDataNew(0.1, 0, 0, 0, z_start + k * h * z_interval, 0, 0, X_start, Y_start, Z, A_start, B_start);
                //JumpDelay
                genDelayData(X_start, Y_start, Z, A_start, B_start, JUMP_DELAY, 0);

                for (int i = 0; i < circle_total; i++)
                {
                    double r_i = a + (i + 1) * 3.14 * b;
                    double T = 2 * 3.14 * r_i / speed;
                    int n_max = Convert.ToInt32((T / t) + 1);
                    for (int n = 0; n < n_max; n++)
                    {
                        double theta = i * 2 * Math.PI + 2 * Math.PI * n / n_max;
                        double r = a + b * theta;
                        double X = X0 + r * Math.Cos(theta);
                        double Y = Y0 + r * Math.Sin(theta);
                        double A = Laser_Taper * Math.Sin(Laser_direction_1 * 3.14 + theta);
                        double B = Laser_Taper * Math.Cos(Laser_direction_1 * 3.14 + theta);
                        double Z1 = Z;
                        if (r >= R_Max)
                        {
                            break;
                        }
                        else
                        {
                            correction(ref X, ref Y, ref Z1, ref A, ref B);
                            if (i == 0)
                            {
                                if (n * 10 < LASER_ON_DELAY)
                                {
                                    DataBuffer.addProcessJumpData(Convert.ToUInt16(X),
                                        Convert.ToUInt16(Y), Convert.ToUInt16(Z1),
                                        Convert.ToUInt16(A), Convert.ToUInt16(B));
                                }
                                else
                                {
                                    DataBuffer.addProcessData(Convert.ToUInt16(X),
                                        Convert.ToUInt16(Y), Convert.ToUInt16(Z1),
                                        Convert.ToUInt16(A), Convert.ToUInt16(B));
                                }
                            }
                            else
                            {
                                DataBuffer.addProcessData(Convert.ToUInt16(X),
                                    Convert.ToUInt16(Y), Convert.ToUInt16(Z1),
                                    Convert.ToUInt16(A), Convert.ToUInt16(B));
                            }
                        }

                    }
                }
            }
            double X_end = X0 + (a + b * theta_max) * Math.Cos(theta_max);
            double Y_end = Y0 + (a + b * theta_max) * Math.Sin(theta_max);
            double A_end = Laser_Taper * Math.Sin(Laser_direction_1 * 3.14 + theta_max);
            double B_end = Laser_Taper * Math.Cos(Laser_direction_1 * 3.14 + theta_max);
            genLineDataNew(JUMP_SPEED, 0, X_end, Y_end, Z, A_end, B_end, 0, 0, Z, 0, 0);
            //JumpDelay
            genDelayData(0, 0, Z, 0, 0, JUMP_DELAY, 0);
            Z = z_start + (k + 1) * h * z_interval;
        }
        genLineDataNew(JUMP_SPEED, 0, 0, 0, Z, 0, 0, 0, 0, 0, 0, 0);
        DataBuffer.addProcessEnd();
    }

    //二维平面等距螺旋线-速度均匀-锥度逐渐变大
    public static void gen2DSpialLineDataNewChangetaper(double speed, double times, double a, double b, double Laser_Taper,
    double R_Max, double X0, double Y0, double z_start, double z_end, double z_interval)
    {

        //计算参数
        double Laser_direction_1;
        double theta_max = (R_Max - a) / b;
        double t = 0.00001;
        int circle_total = Convert.ToInt32(theta_max / (2 * Math.PI));//总扫描圈数

        if (Laser_Taper > 0)
        {
            Laser_direction_1 = 0;
        }
        else
        {
            Laser_direction_1 = 1;
        }

        Laser_Taper = Math.Abs(Laser_Taper);

        //Z轴进给计算
        int h = 0;
        if ((z_end - z_start) > 0)//Z轴运动方向判断
        {
            h = 1;
        }
        else
        {
            h = -1;
        }

        //Z轴扫描总次数(Z轴进给次数)

        int Z_Scantimes = 0;

        if (z_interval == 0)
        {
            Z_Scantimes = 1;
        }
        else
        {
            Z_Scantimes = Convert.ToInt32(Math.Abs(z_end - z_start) / z_interval + 1);

        }

        double Z = z_start;//定义初始Z轴坐标

        DataBuffer.addProcessBegin();

        genLineDataNew(JUMP_SPEED, 0, 0, 0, 0, 0, 0, 0, 0, Z, 0, 0);

        for (int k = 0; k < Z_Scantimes; k++)//Z轴进给循环
        {
            for (int l = 0; l < times; l++)//一个Z轴位置处的扫描循环
            {
                double X_start = X0 + a;
                double Y_start = Y0;
                double A_start = 0;
                double B_start = 0;
                //Jump从原点跳到第一个椭圆的起点,跳转速度设置为0.1，B轴行程太大
                genLineDataNew(0.1, 0, 0, 0, z_start + k * h * z_interval, 0, 0, X_start, Y_start, Z, A_start, B_start);
                //JumpDelay
                genDelayData(X_start, Y_start, Z, A_start, B_start, JUMP_DELAY, 0);

                for (int i = 0; i < circle_total + 1; i++)
                {
                    double r_i = a + (2 * i + 1) * 3.14 * b;
                    double T = 2 * 3.14 * r_i / speed;
                    int n_max = Convert.ToInt32((T / t) + 1);
                    for (int n = 0; n < n_max; n++)
                    {
                        double theta = i * 2 * Math.PI + 2 * Math.PI * n / n_max;
                        double Laser_Taper_i = i * Laser_Taper / circle_total;
                        double r = a + b * theta;
                        double X = X0 + r * Math.Cos(theta);
                        double Y = Y0 + r * Math.Sin(theta);
                        double A = Laser_Taper_i * Math.Sin(Laser_direction_1 * 3.14 + theta);
                        double B = Laser_Taper_i * Math.Cos(Laser_direction_1 * 3.14 + theta);
                        double Z1 = Z;
                        if (r >= R_Max)
                        {
                            break;
                        }
                        else
                        {
                            correction(ref X, ref Y, ref Z1, ref A, ref B);
                            if (i == 0)
                            {
                                if (n * 10 < LASER_ON_DELAY)
                                {
                                    DataBuffer.addProcessJumpData(Convert.ToUInt16(X),
                                        Convert.ToUInt16(Y), Convert.ToUInt16(Z1),
                                        Convert.ToUInt16(A), Convert.ToUInt16(B));
                                }
                                else
                                {
                                    DataBuffer.addProcessData(Convert.ToUInt16(X),
                                        Convert.ToUInt16(Y), Convert.ToUInt16(Z1),
                                        Convert.ToUInt16(A), Convert.ToUInt16(B));
                                }
                            }
                            else
                            {
                                DataBuffer.addProcessData(Convert.ToUInt16(X),
                                    Convert.ToUInt16(Y), Convert.ToUInt16(Z1),
                                    Convert.ToUInt16(A), Convert.ToUInt16(B));
                            }
                        }

                    }
                }
            }
            double X_end = X0 + (a + b * theta_max) * Math.Cos(theta_max);
            double Y_end = Y0 + (a + b * theta_max) * Math.Sin(theta_max);
            double A_end = Laser_Taper * Math.Sin(Laser_direction_1 * 3.14 + theta_max);
            double B_end = Laser_Taper * Math.Cos(Laser_direction_1 * 3.14 + theta_max);
            genLineDataNew(JUMP_SPEED, 0, X_end, Y_end, Z, A_end, B_end, 0, 0, Z, 0, 0);
            //JumpDelay
            genDelayData(0, 0, Z, 0, 0, JUMP_DELAY, 0);
            Z = z_start + (k + 1) * h * z_interval;
        }
        genLineDataNew(JUMP_SPEED, 0, 0, 0, Z, 0, 0, 0, 0, 0, 0, 0);
        DataBuffer.addProcessEnd();
    }


    //二维平面变距螺旋线-速度均匀-锥度逐渐变大
    public static void gen2DSpialLineDataNewChangeSpacingtaper(double speed, double times, double a, double m, double n, double Laser_Taper,
    double R_Max, double X0, double Y0, double z_start, double z_end, double z_interval)
    {

        //计算参数
        double Laser_direction_1;
        double theta_max = (1 / (2 * m)) * (-n + Math.Pow(n * n - 4 * m * (a - R_Max), 0.5));
        double t = 0.00001;
        int circle_total = Convert.ToInt32(theta_max / (2 * Math.PI));//总扫描圈数

        if (Laser_Taper > 0)
        {
            Laser_direction_1 = 0;
        }
        else
        {
            Laser_direction_1 = 1;
        }

        Laser_Taper = Math.Abs(Laser_Taper);

        //Z轴进给计算
        int h = 0;
        if ((z_end - z_start) > 0)//Z轴运动方向判断
        {
            h = 1;
        }
        else
        {
            h = -1;
        }

        //Z轴扫描总次数(Z轴进给次数)

        int Z_Scantimes = 0;

        if (z_interval == 0)
        {
            Z_Scantimes = 1;
        }
        else
        {
            Z_Scantimes = Convert.ToInt32(Math.Abs(z_end - z_start) / z_interval + 1);

        }

        double Z = z_start;//定义初始Z轴坐标

        DataBuffer.addProcessBegin();

        genLineDataNew(JUMP_SPEED, 0, 0, 0, 0, 0, 0, 0, 0, Z, 0, 0);

        for (int k = 0; k < Z_Scantimes; k++)//Z轴进给循环
        {
            for (int l = 0; l < times; l++)//一个Z轴位置处的扫描循环
            {
                double X_start = X0 + a;
                double Y_start = Y0;
                double A_start = 0;
                double B_start = 0;
                //Jump从原点跳到第一个椭圆的起点,跳转速度设置为0.1，B轴行程太大
                genLineDataNew(0.1, 0, 0, 0, z_start + k * h * z_interval, 0, 0, X_start, Y_start, Z, A_start, B_start);
                //JumpDelay
                genDelayData(X_start, Y_start, Z, A_start, B_start, JUMP_DELAY, 0);

                for (int i = 0; i < circle_total + 1; i++)
                {

                    double r_i = a + (2 * i + 1) * n * Math.PI + (2 * i * i + 2 * i + 1) * m * Math.PI * Math.PI;
                    double T = 2 * 3.14 * r_i / speed;
                    int n_max = Convert.ToInt32((T / t) + 1);
                    for (int j = 0; j < n_max; j++)
                    {
                        double theta = i * 2 * Math.PI + 2 * Math.PI * j / n_max;
                        double Laser_Taper_i = i * Laser_Taper / circle_total;
                        double b = n + m * theta;
                        double r = a + b * theta;
                        double X = X0 + r * Math.Cos(theta);
                        double Y = Y0 + r * Math.Sin(theta);
                        double A = Laser_Taper_i * Math.Sin(Laser_direction_1 * 3.14 + theta);
                        double B = Laser_Taper_i * Math.Cos(Laser_direction_1 * 3.14 + theta);
                        double Z1 = Z;

                        if (r >= R_Max)
                        {
                            break;
                        }
                        else
                        {
                            correction(ref X, ref Y, ref Z1, ref A, ref B);
                            if (i == 0)
                            {
                                if (j * 10 < LASER_ON_DELAY)
                                {
                                    DataBuffer.addProcessJumpData(Convert.ToUInt16(X),
                                        Convert.ToUInt16(Y), Convert.ToUInt16(Z1),
                                        Convert.ToUInt16(A), Convert.ToUInt16(B));
                                }
                                else
                                {
                                    DataBuffer.addProcessData(Convert.ToUInt16(X),
                                        Convert.ToUInt16(Y), Convert.ToUInt16(Z1),
                                        Convert.ToUInt16(A), Convert.ToUInt16(B));
                                }
                            }
                            else
                            {
                                DataBuffer.addProcessData(Convert.ToUInt16(X),
                                    Convert.ToUInt16(Y), Convert.ToUInt16(Z1),
                                    Convert.ToUInt16(A), Convert.ToUInt16(B));
                            }
                        }

                    }
                }
            }
            double X_end = X0 + (a + (n + m * theta_max) * theta_max) * Math.Cos(theta_max);
            double Y_end = Y0 + (a + (n + m * theta_max) * theta_max) * Math.Sin(theta_max);
            double A_end = Laser_Taper * Math.Sin(Laser_direction_1 * 3.14 + theta_max);
            double B_end = Laser_Taper * Math.Cos(Laser_direction_1 * 3.14 + theta_max);
            genLineDataNew(JUMP_SPEED, 0, X_end, Y_end, Z, A_end, B_end, 0, 0, Z, 0, 0);
            //JumpDelay
            genDelayData(0, 0, Z, 0, 0, JUMP_DELAY, 0);
            Z = z_start + (k + 1) * h * z_interval;
        }
        genLineDataNew(JUMP_SPEED, 0, 0, 0, Z, 0, 0, 0, 0, 0, 0, 0);
        DataBuffer.addProcessEnd();
    }


    //二维平面变距(m为0时，等间距)螺旋线-速度均匀-锥度逐渐变大-外加一圈修边
    public static void gen2DSpialLineDataNewChangeSpacingtaperRepair(double speed, double times, double a, double m, double n, double Laser_Taper,
    double R_Max, double X0, double Y0, double z_start, double z_end, double z_interval, double repair_times)
    {

        //计算参数
        double Laser_direction_1;
        double theta_max;
        if (m == 0)
        {
            theta_max = (R_Max - a) / n;
        }
        else
        {
            theta_max = (1 / (2 * m)) * (-n + Math.Pow(n * n - 4 * m * (a - R_Max), 0.5));
        }

        double t = 0.00001;
        int circle_total = Convert.ToInt32(theta_max / (2 * Math.PI));//总扫描圈数

        if (Laser_Taper > 0)
        {
            Laser_direction_1 = 0;
        }
        else
        {
            Laser_direction_1 = 1;
        }

        Laser_Taper = Math.Abs(Laser_Taper);

        //Z轴进给计算
        int h = 0;
        if ((z_end - z_start) > 0)//Z轴运动方向判断
        {
            h = 1;
        }
        else
        {
            h = -1;
        }

        //Z轴扫描总次数(Z轴进给次数)

        int Z_Scantimes = 0;

        if (z_interval == 0)
        {
            Z_Scantimes = 1;
        }
        else
        {
            Z_Scantimes = Convert.ToInt32(Math.Abs(z_end - z_start) / z_interval + 1);

        }

        double Z = z_start;//定义初始Z轴坐标

        DataBuffer.addProcessBegin();

        genLineDataNew(JUMP_SPEED, 0, 0, 0, 0, 0, 0, 0, 0, Z, 0, 0);

        for (int k = 0; k < Z_Scantimes; k++)//Z轴进给循环
        {
            for (int l = 0; l < times; l++)//一个Z轴位置处的扫描循环
            {
                double X_start = X0 + a;
                double Y_start = Y0;
                double A_start = 0;
                double B_start = 0;
                //Jump从原点跳到第一个椭圆的起点,跳转速度设置为0.1，B轴行程太大
                genLineDataNew(JUMP_SPEED, 0, 0, 0, z_start + k * h * z_interval, 0, 0, X_start, Y_start, Z, A_start, B_start);
                //JumpDelay
                genDelayData(X_start, Y_start, Z, A_start, B_start, JUMP_DELAY, 0);

                for (int i = 0; i < circle_total + 1; i++)
                {

                    double r_i = a + (2 * i + 1) * n * Math.PI + (2 * i * i + 2 * i + 1) * m * Math.PI * Math.PI;
                    double T = 2 * 3.14 * r_i / speed;
                    int n_max = Convert.ToInt32((T / t) + 1);

                    for (int j = 0; j < n_max; j++)
                    {
                        double theta = i * 2 * Math.PI + 2 * Math.PI * j / n_max;
                        double Laser_Taper_i = (i + j / n_max) * Laser_Taper / circle_total;
                        double b = n + m * theta;
                        double r = a + b * theta;
                        double X = X0 + r * Math.Cos(theta);
                        double Y = Y0 + r * Math.Sin(theta);
                        double A = Laser_Taper_i * Math.Sin(Laser_direction_1 * 3.14 + theta);
                        double B = Laser_Taper_i * Math.Cos(Laser_direction_1 * 3.14 + theta);
                        double Z1 = Z;

                        if (r >= R_Max)
                        {
                            break;
                        }
                        else
                        {
                            correction(ref X, ref Y, ref Z1, ref A, ref B);
                            if (i == 0)
                            {
                                if (j * 10 < LASER_ON_DELAY)
                                {
                                    DataBuffer.addProcessJumpData(Convert.ToUInt16(X),
                                        Convert.ToUInt16(Y), Convert.ToUInt16(Z1),
                                        Convert.ToUInt16(A), Convert.ToUInt16(B));
                                }
                                else
                                {
                                    DataBuffer.addProcessData(Convert.ToUInt16(X),
                                        Convert.ToUInt16(Y), Convert.ToUInt16(Z1),
                                        Convert.ToUInt16(A), Convert.ToUInt16(B));
                                }
                            }
                            else
                            {
                                DataBuffer.addProcessData(Convert.ToUInt16(X),
                                    Convert.ToUInt16(Y), Convert.ToUInt16(Z1),
                                    Convert.ToUInt16(A), Convert.ToUInt16(B));
                            }
                        }

                    }
                }
            }

            double n_repair = (2 * repair_times * Math.PI * R_Max / speed) / t;
            //Log.Information(Convert.ToString("n_repair"));
            //Log.Information(Convert.ToString(n_repair));
            for (int ii = 0; ii < n_repair; ii++)
            {
                double theta_ii = theta_max + (ii / n_repair) * repair_times * 2 * Math.PI;
                //Log.Information(Convert.ToString("theta_ii"));
                //Log.Information(Convert.ToString(theta_ii)); 
                double X_ii = X0 + R_Max * Math.Cos(theta_ii);
                double Y_ii = Y0 + R_Max * Math.Sin(theta_ii);
                double A_ii = Laser_Taper * Math.Sin(Laser_direction_1 * 3.14 + theta_ii);
                double B_ii = Laser_Taper * Math.Cos(Laser_direction_1 * 3.14 + theta_ii);
                double Z1 = Z;
                correction(ref X_ii, ref Y_ii, ref Z1, ref A_ii, ref B_ii);
                DataBuffer.addProcessData(Convert.ToUInt16(X_ii),
                    Convert.ToUInt16(Y_ii), Convert.ToUInt16(Z1),
                    Convert.ToUInt16(A_ii), Convert.ToUInt16(B_ii));
            }

            double X_end = X0 + (a + (n + m * theta_max) * theta_max) * Math.Cos(theta_max);
            double Y_end = Y0 + (a + (n + m * theta_max) * theta_max) * Math.Sin(theta_max);
            double A_end = Laser_Taper * Math.Sin(Laser_direction_1 * 3.14 + theta_max);
            double B_end = Laser_Taper * Math.Cos(Laser_direction_1 * 3.14 + theta_max);
            genLineDataNew(JUMP_SPEED, 0, X_end, Y_end, Z, A_end, B_end, 0, 0, Z, 0, 0);
            //JumpDelay
            genDelayData(0, 0, Z, 0, 0, JUMP_DELAY, 0);
            Z = z_start + (k + 1) * h * z_interval;
        }
        genLineDataNew(JUMP_SPEED, 0, 0, 0, Z, 0, 0, 0, 0, 0, 0, 0);
        DataBuffer.addProcessEnd();
    }

//二维平面变距(m为0时，等间距)螺旋线-速度均匀-锥度逐渐变大-外加一圈修边
    public static void gen2DSpialLineDataNewChangeSpacingtaperRepairChangeR(double speed, double times, double a, double m, double n, double Laser_Taper,
    double R_Max, double Change_R, double X0, double Y0, double z_start, double z_end, double z_interval, double repair_times)
    {

        //计算参数
        double Laser_direction_1;
        double theta_max;


        if (Laser_Taper > 0)
        {
            Laser_direction_1 = 0;
        }
        else
        {
            Laser_direction_1 = 1;
        }

        Laser_Taper = Math.Abs(Laser_Taper);

        //Z轴进给计算
        int h = 0;
        if ((z_end - z_start) > 0)//Z轴运动方向判断
        {
            h = 1;
        }
        else
        {
            h = -1;
        }

        //Z轴扫描总次数(Z轴进给次数)

        int Z_Scantimes = 0;

        if (z_interval == 0)
        {
            Z_Scantimes = 1;
        }
        else
        {
            Z_Scantimes = Convert.ToInt32(Math.Abs(z_end - z_start) / z_interval + 1);

        }

        double Z = z_start;//定义初始Z轴坐标
        double R_helper = R_Max;
        DataBuffer.addProcessBegin();

        genLineDataNew(JUMP_SPEED, 0, 0, 0, 0, 0, 0, 0, 0, Z, 0, 0);

        for (int k = 0; k < Z_Scantimes; k++)//Z轴进给循环
        {
            R_Max = R_helper + k * Change_R;
                    
        if (m == 0)
        {
            theta_max = (R_Max - a) / n;
        }
        else
        {
            theta_max = (1 / (2 * m)) * (-n + Math.Pow(n * n - 4 * m * (a - R_Max), 0.5));
        }

        double t = 0.00001;
        int circle_total = Convert.ToInt32(theta_max / (2 * Math.PI));//总扫描圈数
        
            for (int l = 0; l < times; l++)//一个Z轴位置处的扫描循环
            {
                double X_start = X0 + a;
                double Y_start = Y0;
                double A_start = 0;
                double B_start = 0;
                //Jump从原点跳到第一个椭圆的起点,跳转速度设置为0.1，B轴行程太大
                genLineDataNew(JUMP_SPEED, 0, 0, 0, z_start + k * h * z_interval, 0, 0, X_start, Y_start, Z, A_start, B_start);
                //JumpDelay
                genDelayData(X_start, Y_start, Z, A_start, B_start, JUMP_DELAY, 0);

                for (int i = 0; i < circle_total + 1; i++)
                {

                    double r_i = a + (2 * i + 1) * n * Math.PI + (2 * i * i + 2 * i + 1) * m * Math.PI * Math.PI;
                    double T = 2 * 3.14 * r_i / speed;
                    int n_max = Convert.ToInt32((T / t) + 1);

                    for (int j = 0; j < n_max; j++)
                    {
                        double theta = i * 2 * Math.PI + 2 * Math.PI * j / n_max;
                        double Laser_Taper_i = (i + j / n_max) * Laser_Taper / circle_total;
                        double b = n + m * theta;
                        double r = a + b * theta;
                        double X = X0 + r * Math.Cos(theta);
                        double Y = Y0 + r * Math.Sin(theta);
                        double A = Laser_Taper_i * Math.Sin(Laser_direction_1 * 3.14 + theta);
                        double B = Laser_Taper_i * Math.Cos(Laser_direction_1 * 3.14 + theta);
                        double Z1 = Z;

                        if (r >= R_Max)
                        {
                            break;
                        }
                        else
                        {
                            correction(ref X, ref Y, ref Z1, ref A, ref B);
                            if (i == 0)
                            {
                                if (j * 10 < LASER_ON_DELAY)
                                {
                                    DataBuffer.addProcessJumpData(Convert.ToUInt16(X),
                                        Convert.ToUInt16(Y), Convert.ToUInt16(Z1),
                                        Convert.ToUInt16(A), Convert.ToUInt16(B));
                                }
                                else
                                {
                                    DataBuffer.addProcessData(Convert.ToUInt16(X),
                                        Convert.ToUInt16(Y), Convert.ToUInt16(Z1),
                                        Convert.ToUInt16(A), Convert.ToUInt16(B));
                                }
                            }
                            else
                            {
                                DataBuffer.addProcessData(Convert.ToUInt16(X),
                                    Convert.ToUInt16(Y), Convert.ToUInt16(Z1),
                                    Convert.ToUInt16(A), Convert.ToUInt16(B));
                            }
                        }

                    }
                }
            }

            double n_repair = (2 * repair_times * Math.PI * R_Max / speed) / t;
            //Log.Information(Convert.ToString("n_repair"));
            //Log.Information(Convert.ToString(n_repair));
            for (int ii = 0; ii < n_repair; ii++)
            {
                double theta_ii = theta_max + (ii / n_repair) * repair_times * 2 * Math.PI;
                //Log.Information(Convert.ToString("theta_ii"));
                //Log.Information(Convert.ToString(theta_ii)); 
                double X_ii = X0 + R_Max * Math.Cos(theta_ii);
                double Y_ii = Y0 + R_Max * Math.Sin(theta_ii);
                double A_ii = Laser_Taper * Math.Sin(Laser_direction_1 * 3.14 + theta_ii);
                double B_ii = Laser_Taper * Math.Cos(Laser_direction_1 * 3.14 + theta_ii);
                double Z1 = Z;
                correction(ref X_ii, ref Y_ii, ref Z1, ref A_ii, ref B_ii);
                DataBuffer.addProcessData(Convert.ToUInt16(X_ii),
                    Convert.ToUInt16(Y_ii), Convert.ToUInt16(Z1),
                    Convert.ToUInt16(A_ii), Convert.ToUInt16(B_ii));
            }

            double X_end = X0 + (a + (n + m * theta_max) * theta_max) * Math.Cos(theta_max);
            double Y_end = Y0 + (a + (n + m * theta_max) * theta_max) * Math.Sin(theta_max);
            double A_end = Laser_Taper * Math.Sin(Laser_direction_1 * 3.14 + theta_max);
            double B_end = Laser_Taper * Math.Cos(Laser_direction_1 * 3.14 + theta_max);
            genLineDataNew(JUMP_SPEED, 0, X_end, Y_end, Z, A_end, B_end, 0, 0, Z, 0, 0);
            //JumpDelay
            genDelayData(0, 0, Z, 0, 0, JUMP_DELAY, 0);
            Z = z_start + (k + 1) * h * z_interval;
        }
        genLineDataNew(JUMP_SPEED, 0, 0, 0, Z, 0, 0, 0, 0, 0, 0, 0);
        DataBuffer.addProcessEnd();
    }


    //切内圆
    public static void genCircleCutinsideData(double speed, int times, double X1,
            double Y1, double X2, double Y2, int M, double angle, double taper, double r_max, double r_interval, double z_start,
            double z_end, double z_interval, int circle_num_repair, int times_repair)
    {
        //数据生成准备
        double r_min = Math.Sqrt(Math.Pow(X2 - X1, 2) + Math.Pow(Y2 - Y1, 2));
        int circle_num = Convert.ToInt32((r_max - r_min) / r_interval + 1);
        double r_max_real = r_min + (circle_num - 1) * r_interval;
        double L_I;//子循环激光锥度
        L_I = taper;//初始扫描最内圈激光锥度

        //Z轴进给计算
        int h = 0;
        if ((z_end - z_start) > 0)//Z轴运动方向判断
        {
            h = 1;
        }
        else
        {
            h = -1;
        }

        //Z轴扫描总次数(Z轴进给次数)

        int Z_Scantimes = 0;

        if (z_interval == 0)
        {
            Z_Scantimes = 1;
        }
        else
        {
            Z_Scantimes = Convert.ToInt32(Math.Abs(z_end - z_start) / z_interval + 1);
        }

        DataBuffer.addProcessBegin();

        double Z = z_start;//定义初始Z轴坐标

        genLineDataNew(JUMP_SPEED, 0, 0, 0, 0, 0, 0, 0, 0, Z, 0, 0);

        for (int k = 0; k < Z_Scantimes; k++)//Z轴进给循环
        {
            for (int j = 0; j < times; j++)//一个Z轴位置处的扫描循环
            {
                double X = X1 + r_min;
                double Y = Y1;
                double A = 0;
                double B = taper;

                //Jump从原点跳到第一个圆的起点
                genLineDataNew(JUMP_SPEED, 0, 0, 0, z_start + k * h * z_interval, 0, 0, X, Y, Z, A, B);
                //JumpDelay
                genDelayData(X, Y, Z, A, B, JUMP_DELAY, 0);

                //主循环-由内到外
                int i = 0;
                while (i < circle_num)//主循环-由内到外
                {

                    L_I = taper;//- i * taper / circle_num;//子循环锥度
                    B = L_I;

                    //MarkCircle
                    genCircleNew(X1, Y1, X, Y, Z, L_I, speed);
                    //MarkDelay
                    genDelayData(X, Y, Z, A, B, MARK_DELAY, LASER_OFF_DELAY);
                    i = i + 1;

                    double X_Temp = X;
                    double B_Temp = B;

                    X = X + r_interval;//子循环X坐标变化
                    B = taper;//(i+1) * taper / circle_num;

                    if (i == circle_num)//如果所有循环扫描完成，则跳转回原点，并退出循环
                    {
                        //Jump
                        genLineDataNew(JUMP_SPEED, 0, X_Temp, Y, Z, A, B_Temp, X1, Y1, Z, 0, 0);
                        break;
                    }
                    else//否则跳转到下一个圆的起点
                        //Jump
                        genLineDataNew(JUMP_SPEED, 0, X_Temp, Y, Z, A, B_Temp, X, Y, Z, A, B);
                    //JumpDelay
                    genDelayData(X, Y, Z, A, B, JUMP_DELAY, 0);
                }

                //修边
                for (int l = 0; l < times_repair; l++)
                {
                    X = X1 + r_min;
                    Y = Y1;
                    A = 0;
                    B = taper;
                    //Jump从原点跳到修边最内圈起点
                    genLineDataNew(JUMP_SPEED, 0, X1, Y1, Z, 0, 0, X, Y, Z, A, B);
                    //JumpDelay
                    genDelayData(X, Y, Z, A, B, JUMP_DELAY, 0);

                    for (int m = 0; m < circle_num_repair; m++)
                    {

                        L_I = taper;//子循环锥度
                        B = L_I;

                        //MarkCircle
                        genCircleNew(X1, Y1, X, Y, Z, L_I, speed);
                        //MarkDelay
                        genDelayData(X, Y, Z, A, B, MARK_DELAY, LASER_OFF_DELAY);

                        double X_Temp = X;
                        double B_Temp = B;

                        X = X + r_interval;//子循环X坐标变化
                        B = taper;

                        if (m == circle_num_repair - 1)//如果所有循环扫描完成，则跳转回原点，并退出循环
                        {
                            //Jump
                            genLineDataNew(JUMP_SPEED, 0, X_Temp, Y, Z, A, B_Temp, X1, Y1, Z, 0, 0);
                            break;
                        }
                        else//否则跳转到下一个圆的起点
                            //Jump
                            genLineDataNew(JUMP_SPEED, 0, X_Temp, Y, Z, A, B_Temp, X, Y, Z, A, B);
                        //JumpDelay
                        genDelayData(X, Y, Z, A, B, JUMP_DELAY, 0);
                    }
                }
                Z = z_start + (k + 1) * h * z_interval;
            }
        }
        //Jump
        genLineDataNew(JUMP_SPEED, 0, 0, 0, Z, 0, 0, 0, 0, 0, 0, 0);
        DataBuffer.addProcessEnd();
    }


    //三维圆柱螺旋线
    public static void gen3DCylindricalhelixData(double speed, double X1,
            double Y1, double X2, double Y2, double taper, double z_start,
            double z_end, double z_interval)
    {
        //数据生成准备
        double r = Math.Sqrt(Math.Pow(X2 - X1, 2) + Math.Pow(Y2 - Y1, 2));

        int Laser_direction_1;

        if (taper > 0)
        {
            Laser_direction_1 = 0;
        }
        else
        {
            Laser_direction_1 = 1;
        }

        taper = Math.Abs(taper);


        //Z轴进给方向
        int h = 0;
        if ((z_end - z_start) > 0)//Z轴运动方向判断
        {
            h = 1;
        }
        else
        {
            h = -1;
        }

        //总圈数
        int circle_total = Convert.ToInt32(Math.Abs(z_start - z_end) / z_interval);

        double t;//数据间隔时间10μs
        t = 0.00001;

        //总扫描时间
        double T;
        T = 2 * Math.PI * r * circle_total / speed;

        int n_max = Convert.ToInt32((T / t) + 1);
        int n = 0;

        double X_start = X2;
        double Y_start = Y1;
        double A_start = 0;
        double B_start = taper * Math.Cos(Laser_direction_1 * 3.14);

        //Jump从原点跳到扫描起点
        genLineDataNew(JUMP_SPEED, 0, 0, 0, 0, 0, 0, X_start, Y_start, z_start, A_start, B_start);
        //JumpDelay
        genDelayData(X_start, Y_start, z_start, A_start, B_start, JUMP_DELAY, 0);

        DataBuffer.addProcessBegin();

        while (n < n_max)
        {
            double X = X_start;
            double Y = Y_start;
            double A = A_start;
            double B = B_start;
            double Z = z_start;

            X = X1 + r * Math.Cos(n * 2 * 3.14 * circle_total / n_max);//第n个数据点X坐标
            Y = Y1 + r * Math.Sin(n * 2 * 3.14 * circle_total / n_max);//第n个数据点Y坐标
            Z = z_start + n * h * Math.Abs(z_start - z_end) / n_max;
            A = taper * Math.Sin(Laser_direction_1 * 3.14 + n * 2 * 3.14 * circle_total / n_max);
            B = taper * Math.Cos(Laser_direction_1 * 3.14 + n * 2 * 3.14 * circle_total / n_max);

            correction(ref X, ref Y, ref Z, ref A, ref B);
            if (n * 10 < LASER_ON_DELAY)
            {

                DataBuffer.addProcessJumpData(Convert.ToUInt16(X),
                                Convert.ToUInt16(Y), Convert.ToUInt16(Z),
                                Convert.ToUInt16(A), Convert.ToUInt16(B));
            }
            else
            {
                DataBuffer.addProcessData(Convert.ToUInt16(X),
                                Convert.ToUInt16(Y), Convert.ToUInt16(Z),
                                Convert.ToUInt16(A), Convert.ToUInt16(B));
            }
            n = n + 1;
        }
        double X_end = X2;
        double Y_end = Y1;
        double A_end = 0;
        double B_end = taper * Math.Cos(Laser_direction_1 * 3.14);
        //Jump
        genLineDataNew(JUMP_SPEED, 0, X_end, Y_end, z_end, A_end, B_end, 0, 0, 0, 0, 0);

        DataBuffer.addProcessEnd();
    }


    //三维锥形等间距螺旋线
    public static void gen3DconehelixData(double speed, double X1,
            double Y1, double X2, double Y2, double taper_hole, double taper_laser, double z_start,
            double z_end, double z_interval)
    {
        //数据生成准备
        double a = Math.Abs(X2 - X1);
        double r_max = a + Math.Tan(taper_hole * 3.14 / 180);
        double z_length = Math.Abs(z_end - z_start);
        double theta_max = 2 * 3.14 * z_length / z_interval;
        double b = (r_max - a) / theta_max;

        double r = Math.Sqrt(Math.Pow(X2 - X1, 2) + Math.Pow(Y2 - Y1, 2));
        double l_total = (1 / (3 * b)) * (Math.Pow((Math.Pow(b, 2) + Math.Pow(a + b * theta_max, 2)), 1.5) -
        Math.Pow((Math.Pow(a, 2) + Math.Pow(b, 2)), 1.5));//螺旋线弧长        


        int Laser_direction_1;

        if (taper_laser > 0)
        {
            Laser_direction_1 = 0;
        }
        else
        {
            Laser_direction_1 = 1;
        }

        taper_laser = Math.Abs(taper_laser);


        //Z轴进给方向
        int h = 0;
        if ((z_end - z_start) > 0)//Z轴运动方向判断
        {
            h = 1;
        }
        else
        {
            h = -1;
        }

        //总圈数
        int circle_total = Convert.ToInt32(Math.Abs(z_start - z_end) / z_interval);

        double t;//数据间隔时间10μs
        t = 0.00001;

        //总扫描时间
        double T;
        T = l_total / speed;

        int n_max = Convert.ToInt32((T / t) + 1);
        int n = 0;

        double X_start = X2;
        double Y_start = 0;
        double A_start = 0;
        double B_start = taper_laser * Math.Cos(Laser_direction_1 * 3.14);

        //Jump从原点跳到扫描起点
        genLineDataNew(JUMP_SPEED, 0, 0, 0, 0, 0, 0, X_start, Y_start, z_start, A_start, B_start);
        //JumpDelay
        genDelayData(X_start, Y_start, z_start, A_start, B_start, JUMP_DELAY, 0);

        DataBuffer.addProcessBegin();

        while (n < n_max)
        {
            double X = X_start;
            double Y = Y_start;
            double A = A_start;
            double B = B_start;
            double Z = z_start;
            double theta = n * 2 * 3.14 * circle_total / n_max;
            double r_helper = a + b * theta;

            X = X1 + r_helper * Math.Cos(theta);//第n个数据点X坐标
            Y = Y1 + r_helper * Math.Sin(theta);//第n个数据点Y坐标
            Z = z_start + n * h * Math.Abs(z_start - z_end) / n_max;
            A = taper_laser * Math.Sin(Laser_direction_1 * 3.14 + theta);
            B = taper_laser * Math.Cos(Laser_direction_1 * 3.14 + theta);

            correction(ref X, ref Y, ref Z, ref A, ref B);
            if (n * 10 < LASER_ON_DELAY)
            {

                DataBuffer.addProcessJumpData(Convert.ToUInt16(X),
                                Convert.ToUInt16(Y), Convert.ToUInt16(Z),
                                Convert.ToUInt16(A), Convert.ToUInt16(B));
            }
            else
            {
                DataBuffer.addProcessData(Convert.ToUInt16(X),
                                Convert.ToUInt16(Y), Convert.ToUInt16(Z),
                                Convert.ToUInt16(A), Convert.ToUInt16(B));
            }
            n = n + 1;
        }
        double X_end = X1 + a + Math.Tan(taper_hole * 3.14 / 180);
        double Y_end = 0;
        double A_end = 0;
        double B_end = taper_laser * Math.Cos(Laser_direction_1 * 3.14);
        //Jump
        genLineDataNew(JUMP_SPEED, 0, X_end, Y_end, z_end, A_end, B_end, 0, 0, 0, 0, 0);

        DataBuffer.addProcessEnd();
    }

    //填充同心圆，锥度不变
    public static void genCircleSametaperData(double speed, int times, double X1, double Y1, double X2, double Y2,
    double taper, double r_min, double r_interval, double z_start, int circle_num_repair, int times_repair)
    {
        //数据生成准备
        double r_max = Math.Sqrt(Math.Pow(X2 - X1, 2) + Math.Pow(Y2 - Y1, 2));
        int circle_num = Convert.ToInt32((r_max - r_min) / r_interval + 1);
        double r_min_real = r_max - (circle_num - 1) * r_interval;
        double Z = z_start;//定义初始Z轴坐标

        double X = X1 + r_min_real;
        double Y = Y1;
        double A = 0;
        double B = taper;

        //Jump从原点跳到第一个圆的起点
        genLineDataNew(JUMP_SPEED, 0, 0, 0, Z, 0, 0, X, Y, Z, A, B);
        //JumpDelay
        genDelayData(X, Y, Z, A, B, JUMP_DELAY, 0);

        //主循环-由内到外
        int i = 0;
        while (i < circle_num)//主循环-由内到外
        {
            //MarkCircle
            genCircleNew(X1, Y1, X, Y, Z, taper, speed);
            //MarkDelay
            genDelayData(X, Y, Z, A, B, MARK_DELAY, LASER_OFF_DELAY);
            i = i + 1;

            double X_Temp = X;

            X = X + r_interval;//子循环X坐标变化

            if (i == circle_num)//如果所有循环扫描完成，则跳转回原点，并退出循环
            {
                //Jump
                genLineDataNew(JUMP_SPEED, 0, X_Temp, Y, Z, A, B, 0, 0, Z, 0, 0);
                break;
            }
            else//否则跳转到下一个圆的起点
                //Jump
                genLineDataNew(JUMP_SPEED, 0, X_Temp, Y, Z, A, B, X, Y, Z, A, B);
            //JumpDelay
            genDelayData(X, Y, Z, A, B, JUMP_DELAY, 0);
        }
        //修边
        for (int l = 0; l < times_repair; l++)
        {

            X = X2 - (circle_num_repair - 1) * r_interval;
            Y = Y1;
            B = taper;
            //Jump从原点跳到修边最内圈起点
            genLineDataNew(JUMP_SPEED, 0, 0, 0, Z, 0, 0, X, Y, Z, A, B);
            //JumpDelay
            genDelayData(X, Y, Z, A, B, JUMP_DELAY, 0);

            for (int m = 0; m < circle_num_repair; m++)
            {

                //MarkCircle
                genCircleNew(X1, Y1, X, Y, Z, B, speed);
                //MarkDelay
                genDelayData(X, Y, Z, A, B, MARK_DELAY, LASER_OFF_DELAY);

                double X_Temp = X;

                X = X + r_interval;//子循环X坐标变化

                if (m == circle_num_repair - 1)//如果所有循环扫描完成，则跳转回原点，并退出循环
                {
                    //Jump
                    genLineDataNew(JUMP_SPEED, 0, X_Temp, Y, Z, A, B, 0, 0, Z, 0, 0);
                    break;
                }
                else//否则跳转到下一个圆的起点
                    //Jump
                    genLineDataNew(JUMP_SPEED, 0, X_Temp, Y, Z, A, B, X, Y, Z, A, B);
                //JumpDelay
                genDelayData(X, Y, Z, A, B, JUMP_DELAY, 0);
            }
        }
    }


    //三维填充同心圆
    public static void gen3Dfilledcircle(double speed, double X0, double Y0, double X_start, double taper,
    double laser_taper, double r_interval, double z_start, double z_end, double z_interval, int times,
    int circle_num_repair, int times_repair)
    {
        //Z轴进给计算
        int h = 0;
        if ((z_end - z_start) > 0)//Z轴运动方向判断
        {
            h = 1;
        }
        else
        {
            h = -1;
        }

        //Z轴扫描总次数(Z轴进给次数)

        int Z_Scantimes = 0;

        if (z_interval == 0)
        {
            Z_Scantimes = 1;
        }
        else
        {
            Z_Scantimes = Convert.ToInt32(Math.Abs(z_end - z_start) / z_interval + 1);
        }

        DataBuffer.addProcessBegin();
        //Jump从原点跳到第一个圆的起点
        genLineDataNew(JUMP_SPEED, 0, 0, 0, 0, 0, 0, 0, 0, z_start, 0, 0);
        //JumpDelay
        genDelayData(0, 0, z_start, 0, 0, JUMP_DELAY, 0);

        for (int i = 0; i < Z_Scantimes; i++)//Z轴进给循环
        {

            double z_i = z_start + h * i * z_interval;
            double r_min_i = X_start - Math.Abs(z_start - z_i) * Math.Tan(taper * 3.14 / 180);
            if (i > 0)
            {
                //Jump从原点跳到第一个圆的起点
                genLineDataNew(JUMP_SPEED, 0, 0, 0, z_start + h * (i - 1) * z_interval, 0, 0, 0, 0, z_i, 0, 0);
                //JumpDelay
                genDelayData(0, 0, z_i, 0, 0, JUMP_DELAY, 0);
            }

            genCircleSametaperData(speed, times, X0, Y0, X_start, 0, laser_taper, r_min_i, r_interval, z_i, circle_num_repair, times_repair);

        }

        //Jump从原点跳到第一个圆的起点
        genLineDataNew(JUMP_SPEED, 0, 0, 0, z_end, 0, 0, 0, 0, 0, 0, 0);
        DataBuffer.addProcessEnd();
    }


    //非对称AOI矩形
    public static void genRectangleDataUnsymmertical(double X0, double Y0, double X1, double Y1,
           double Z1, double Laser_Taper_X1, double Laser_Taper_X2, double Laser_Taper_Y1, double Laser_Taper_Y2, double V)
    {
        double X_length = Math.Abs(2 * (X1 - X0));
        double Y_length = Math.Abs(2 * (Y1 - Y0));
        double X_Start = X1;
        double Y_Start = Y1;
        double Z_Start = Z1;
        double A1_Start = Laser_Taper_Y1;
        double A2_Start = Laser_Taper_Y2;
        double B1_Start = Laser_Taper_X1;
        double B2_Start = Laser_Taper_X2;
        double V1 = V;

        genLineDataNew(V1, 1, X_Start, Y_Start, Z_Start, A1_Start, B1_Start, X_Start - X_length, Y_Start, Z_Start, A1_Start, -B1_Start);
        //MarkDelay
        genDelayData(X_Start - X_length, Y_Start, Z_Start, A1_Start, -B1_Start, MARK_DELAY, LASER_OFF_DELAY);

        //genDelayData(X_Start - X_length, Y_Start, Z_Start, A1_Start, - B1_Start, POLYGON_DELAY, POLYGON_DELAY);

        //Jump
        genLineDataNew(JUMP_SPEED, 0, X_Start - X_length, Y_Start, Z_Start, A1_Start, -B1_Start, X_Start - X_length, Y_Start, Z_Start, A2_Start, -B1_Start);
        //JumpDelay
        genDelayData(X_Start - X_length, Y_Start, Z_Start, A2_Start, -B1_Start, JUMP_DELAY, 0);

        genLineDataNew(V1, 1, X_Start - X_length, Y_Start, Z_Start, A2_Start, -B1_Start, X_Start - X_length, Y_Start - Y_length, Z_Start, -A2_Start, -B1_Start);
        //MarkDelay
        genDelayData(X_Start - X_length, Y_Start - Y_length, Z_Start, -A2_Start, -B1_Start, MARK_DELAY, LASER_OFF_DELAY);
        //genDelayData(X_Start - X_length, Y_Start - Y_length, Z_Start, - A1_Start, - B1_Start, POLYGON_DELAY, POLYGON_DELAY);

        //Jump
        genLineDataNew(JUMP_SPEED, 0, X_Start - X_length, Y_Start - Y_length, Z_Start, -A2_Start, -B1_Start, X_Start - X_length, Y_Start - Y_length, Z_Start, -A2_Start, -B2_Start);
        //JumpDelay
        genDelayData(X_Start - X_length, Y_Start - Y_length, Z_Start, -A2_Start, -B2_Start, JUMP_DELAY, 0);

        genLineDataNew(V1, 1, X_Start - X_length, Y_Start - Y_length, Z_Start, -A2_Start, -B2_Start, X_Start, Y_Start - Y_length, Z_Start, -A2_Start, B2_Start);
        //MarkDelay
        genDelayData(X_Start, Y_Start - Y_length, Z_Start, -A2_Start, B2_Start, MARK_DELAY, LASER_OFF_DELAY);
        //genDelayData(X_Start, Y_Start - Y_length, Z_Start, - A2_Start, B2_Start, POLYGON_DELAY, POLYGON_DELAY);

        //Jump
        genLineDataNew(JUMP_SPEED, 0, X_Start, Y_Start - Y_length, Z_Start, -A2_Start, B2_Start, X_Start, Y_Start - Y_length, Z_Start, -A1_Start, B1_Start);
        //JumpDelay
        genDelayData(X_Start, Y_Start - Y_length, Z_Start, -A1_Start, B1_Start, JUMP_DELAY, 0);

        genLineDataNew(V1, 1, X_Start, Y_Start - Y_length, Z_Start, -A1_Start, B1_Start, X_Start, Y_Start, Z_Start, A1_Start, B1_Start);
        //MarkDelay
        genDelayData(X_Start, Y_Start, Z_Start, A1_Start, B1_Start, MARK_DELAY, LASER_OFF_DELAY);
        //genDelayData(X_Start, Y_Start, Z_Start, A2_Start, B2_Start, MARK_DELAY, LASER_OFF_DELAY);
    }

    //非对称AOI矩形填充
    public static void genFilledRectangleDataUnsymmertical(double X0, double Y0, double X1, double Y1, double taper_A1_Max, double taper_A2_Max,
        double taper_B1_Max, double taper_B2_Max, double FeedSpacing_X, double FeedSpacing_Y, double ScanSpeed,
        double z_start, double z_end, double z_interval, int times, double X2, double Y2, int circle_num_repair, int times_repair)
    {
        int Num_of_rectangles_x = Convert.ToInt32(Math.Abs(X1 - X2) / FeedSpacing_X) + 1;//x方向扫描矩形数
        int Num_of_rectangles_y = Convert.ToInt32(Math.Abs(Y1 - Y2) / FeedSpacing_Y) + 1;//y方向扫描矩形数
        int Num_of_rectangles = Num_of_rectangles_x < Num_of_rectangles_y ? Num_of_rectangles_x : Num_of_rectangles_y;//扫描矩形数
        double X_Min_Real = X1 - (Num_of_rectangles - 1) * FeedSpacing_X;//实际最内矩形起点X坐标
        double Y_Min_Real = Y1 - (Num_of_rectangles - 1) * FeedSpacing_Y;//实际最内矩形起点Y坐标
        double B1_Min_Real = taper_B1_Max / Num_of_rectangles_x;//实际最内矩形B轴1
        double B2_Min_Real = taper_B2_Max / Num_of_rectangles_x;//实际最内矩形B轴2       
        double A1_Min_Real = taper_A1_Max / Num_of_rectangles_y;//实际最内矩形A轴1
        double A2_Min_Real = taper_A2_Max / Num_of_rectangles_y;//实际最内矩形A轴2
        //Z轴进给计算
        int h = 0;
        if ((z_end - z_start) > 0)//Z轴运动方向判断
        {
            h = 1;
        }
        else
        {
            h = -1;
        }
        //Z轴扫描总次数
        int Z_Scantimes = 0;
        if (z_interval == 0)
        {
            Z_Scantimes = 1;
        }
        else
        {
            Z_Scantimes = Convert.ToInt32(Math.Abs(z_end - z_start) / z_interval + 1);
        }
        DataBuffer.addProcessBegin();
        double Z = z_start;//定义初始Z轴坐标
        genLineDataNew(JUMP_SPEED, 0, 0, 0, 0, 0, 0, 0, 0, Z, 0, 0);
        genDelayData(0, 0, Z, 0, 0, JUMP_DELAY, 0);

        for (int k = 0; k < Z_Scantimes; k++)//Z轴进给循环
        {
            for (int j = 0; j < times; j++)//一个Z轴位置处的扫描循环
            {
                double X = X_Min_Real;
                double Y = Y_Min_Real;
                double A1 = A1_Min_Real;
                double A2 = A2_Min_Real;
                double B1 = B1_Min_Real;
                double B2 = B2_Min_Real;

                //Jump从原点跳到第一个方的起点
                genLineDataNew(JUMP_SPEED, 0, 0, 0, z_start + k * h * z_interval, 0, 0, X, Y, Z, A1, B1);
                //JumpDelay
                genDelayData(X, Y, Z, A1, B1, JUMP_DELAY, 0);

                //主循环-由内到外
                int i = 0;
                while (i < Num_of_rectangles)//主循环-由内到外
                {
                    X = X_Min_Real + i * FeedSpacing_X;
                    Y = Y_Min_Real + i * FeedSpacing_Y;
                    A1 = A1_Min_Real + i * (taper_A1_Max / Num_of_rectangles);
                    A2 = A2_Min_Real + i * (taper_A2_Max / Num_of_rectangles);
                    B1 = B1_Min_Real + i * (taper_B1_Max / Num_of_rectangles);
                    B2 = B2_Min_Real + i * (taper_B2_Max / Num_of_rectangles);


                    //MarkRectangle
                    genRectangleDataUnsymmertical(X0, Y0, X, Y, Z, B1, B2, A1, A2, ScanSpeed);//B轴为X轴的锥度；A轴为Y轴的锥度
                    //MarkDelay
                    genDelayData(X, Y, Z, A2, B2, MARK_DELAY, LASER_OFF_DELAY);

                    i = i + 1;

                    double X_Temp = X;
                    double Y_Temp = Y;
                    double A_Temp = A2;
                    double B_Temp = B2;

                    X = X + FeedSpacing_X;
                    Y = Y + FeedSpacing_Y;
                    A1 = A1 + taper_A1_Max / Num_of_rectangles;
                    A2 = A2 + taper_A2_Max / Num_of_rectangles;
                    B1 = B1 + taper_B1_Max / Num_of_rectangles;
                    B2 = B2 + taper_B2_Max / Num_of_rectangles;

                    if (i == Num_of_rectangles)//如果所有循环扫描完成，则跳转回原点，并退出循环
                    {
                        //Jump
                        genLineDataNew(JUMP_SPEED, 0, X_Temp, Y_Temp, Z, A_Temp, B_Temp, X0, Y0, Z, 0, 0);
                        break;
                    }
                    else//否则跳转到下一个矩形的起点

                        //Jump
                        genLineDataNew(JUMP_SPEED, 0, X_Temp, Y_Temp, Z, A_Temp, B_Temp, X, Y, Z, A1, B1);
                    //JumpDelay
                    genDelayData(X, Y, Z, A1, B1, JUMP_DELAY, 0);
                }

                for (int l = 0; l < times_repair; l++)
                {

                    X = X1 - (circle_num_repair - 1) * FeedSpacing_X;
                    Y = Y1 - (circle_num_repair - 1) * FeedSpacing_Y;
                    A1 = (Num_of_rectangles - circle_num_repair) * taper_A1_Max / Num_of_rectangles;
                    B1 = (Num_of_rectangles - circle_num_repair) * taper_B1_Max / Num_of_rectangles;
                    //Jump从原点跳到修边最内圈起点
                    genLineDataNew(JUMP_SPEED, 0, X0, Y0, Z, 0, 0, X, Y, Z, A1, B1);
                    //JumpDelay
                    genDelayData(X, Y, Z, A1, B1, JUMP_DELAY, 0);

                    for (int m = 0; m < circle_num_repair; m++)
                    {
                        X = X1 - (circle_num_repair - 1) * FeedSpacing_X + m * FeedSpacing_X;
                        Y = Y1 - (circle_num_repair - 1) * FeedSpacing_Y + m * FeedSpacing_Y;
                        A1 = (Num_of_rectangles - circle_num_repair) * taper_A1_Max / Num_of_rectangles + m * taper_A1_Max / Num_of_rectangles;
                        A2 = (Num_of_rectangles - circle_num_repair) * taper_A2_Max / Num_of_rectangles + m * taper_A2_Max / Num_of_rectangles;
                        B1 = (Num_of_rectangles - circle_num_repair) * taper_B1_Max / Num_of_rectangles + m * taper_B1_Max / Num_of_rectangles;
                        B2 = (Num_of_rectangles - circle_num_repair) * taper_B2_Max / Num_of_rectangles + m * taper_B2_Max / Num_of_rectangles;


                        //MarkRectangle
                        genRectangleDataUnsymmertical(X0, Y0, X, Y, Z, B1, B2, A1, A2, ScanSpeed);//B轴为X轴的锥度；A轴为Y轴的锥度
                        //MarkDelay
                        genDelayData(X, Y, Z, A2, B2, MARK_DELAY, LASER_OFF_DELAY);

                        double X_Temp = X;
                        double Y_Temp = Y;
                        double A_Temp = A2;
                        double B_Temp = B2;

                        X = X + FeedSpacing_X;
                        Y = Y + FeedSpacing_Y;
                        A1 = A1 + taper_A1_Max / Num_of_rectangles;
                        A2 = A2 + taper_A2_Max / Num_of_rectangles;
                        B1 = B1 + taper_B2_Max / Num_of_rectangles;
                        B2 = B2 + taper_B2_Max / Num_of_rectangles;

                        if (m == circle_num_repair - 1)//如果所有循环扫描完成，则跳转回原点，并退出循环
                        {
                            //Jump
                            genLineDataNew(JUMP_SPEED, 0, X_Temp, Y_Temp, Z, A_Temp, B_Temp, X0, Y0, Z, 0, 0);
                            break;
                        }
                        else//否则跳转到下一个矩形的起点

                            //Jump
                            genLineDataNew(JUMP_SPEED, 0, X_Temp, Y_Temp, Z, A_Temp, B_Temp, X, Y, Z, A1, B1);
                        //JumpDelay
                        genDelayData(X, Y, Z, A1, B1, JUMP_DELAY, 0);
                    }
                }
                Z = z_start + (k + 1) * h * z_interval;
            }
        }
        //Jump
        genLineDataNew(JUMP_SPEED, 0, 0, 0, Z, 0, 0, 0, 0, 0, 0, 0);
        DataBuffer.addProcessEnd();
    }


    //斜方孔加工
    public static void genProcessSquareHole()
    {

        //double Z_Start = 3;
        //double z_end = -2.8;
        //double z_interval = 0.04;
        double Z_Start = 3;
        double z_end = -1.2;
        double z_interval = 0.06;
        double Z_Scantimes = (Z_Start - z_end) / z_interval;
        for (int n = 0; n < Z_Scantimes + 1; n++)
        {
            double X0 = 0;
            double Y0 = 1.25 - 0.025 - (n + 1) * 0.009;//0.01
            double X2 = 0.01;
            double Y2 = 1.25 - 0.025 - (n + 1) * 0.009 + 0.01;
            double Z_Start_1 = Z_Start - n * z_interval;
            genFilledRectangleDataUnsymmertical(X0, Y0, 2.5, 1.25, -4, 0, -4, -4, 0.015, 0.015, 500, Z_Start_1, Z_Start_1, 0, 1, X2, Y2, 1, 1);
        }
    }


    //直线步进扫描(晶体加工)
    public static void genProcessLineMoveZ(double speed, double X1, double Y1, double X2, double Y2, double z_start, double z_end, double z_interval)
    {
        //Z轴进给计算
        int h = 0;
        if ((z_end - z_start) > 0)//Z轴运动方向判断
        {
            h = 1;
        }
        else
        {
            h = -1;
        }

        //Z轴扫描总次数(Z轴进给次数)

        int Z_Scantimes = 0;

        if (z_interval == 0)
        {
            Z_Scantimes = 1;
        }
        else
        {
            Z_Scantimes = Convert.ToInt32(Math.Abs(z_end - z_start) / z_interval + 1);
        }

        DataBuffer.addProcessBegin();
        //Jump从原点跳到Z轴起点
        genLineDataNew(JUMP_SPEED, 0, 0, 0, 0, 0, 0, 0, 0, z_start, 0, 0);
        //JumpDelay
        genDelayData(0, 0, z_start, 0, 0, JUMP_DELAY, 0);
       
        for (int i = 0; i < Z_Scantimes; i++)//Z轴进给循环
        {
            double z_i = z_start + h * i * z_interval;
            genLineDataNew(speed, 1, X1, Y1, z_i, 0, 0, X2, Y2, z_i, 0, 0);
            genLineDataNew(speed, 1, X2, Y2, z_i, 0, 0, X1, Y1, z_i, 0, 0);

            //genLineDataNew(JUMP_SPEED, 0, X1, Y1, z_i, 0, 0, X1, Y1, z_start + h * (i+1) * z_interval, 0, 0);
        }

        //Jump从原点跳到第一个圆的起点
        genLineDataNew(JUMP_SPEED, 0, X1, Y1, z_end, 0, 0, 0, 0, 0, 0, 0);
        DataBuffer.addProcessEnd();
    }

    //平面直线往返扫描(刻槽)
    public static void genProcessLineGoAndBack(double speed, double X_start, double Y_start, double length, double interval, int num)
    {
        DataBuffer.addProcessBegin();

        //Jump从原点跳到扫描直线的起点
        genLineDataNew(JUMP_SPEED, 0, 0, 0, 0, 0, 0, X_start, Y_start, 0, 0, 0);
        //JumpDelay
        genDelayData(X_start, Y_start, 0, 0, 0, JUMP_DELAY, 0);
        
        for (int i = 0; i < num / 2; i++)
        {
            double X_0 = X_start;
            double Y_0 = Y_start - i * interval * 2;
            genLineDataNew(speed, 1, X_0, Y_0, 0, 0, 0, X_0 + length, Y_0, 0, 0, 0);
            //MarkDelay
            genDelayData(X_0 + length, Y_0, 0, 0, 0, MARK_DELAY, 0);
            genLineDataNew(speed, 0, X_0 + length, Y_0, 0, 0, 0, X_0 + length, Y_0 - interval, 0, 0, 0);              
            //JumpDelay
            genDelayData(X_0 + length, Y_0 - interval, 0, 0, 0, JUMP_DELAY, 0);
            genLineDataNew(speed, 1, X_0 + length, Y_0 - interval, 0, 0, 0, X_0, Y_0 - interval, 0, 0, 0);
            //MarkDelay
            genDelayData(X_0, Y_0 - interval, 0, 0, 0, MARK_DELAY, 0);
        }
        
        genLineDataNew(speed, 0, X_start, Y_start - (num / 2 - 1) * interval * 2, 0, 0, 0, 0, 0, 0, 0, 0);   
        DataBuffer.addProcessEnd();
    }



    //直线往返扫描带Z轴进给(大幅面五轴刻蚀)
    public static void genProcessLineGoAndBack5D(double speed,double times, double X_start, double Y_start, double length, double interval, int num, double A, double B,
     double z_start, double z_end, double z_interval)
    {
        DataBuffer.addProcessBegin();
        //Z轴进给计算
        int h = 0;
        if ((z_end - z_start) > 0)//Z轴运动方向判断
        {
            h = 1;
        }
        else
        {
            h = -1;
        }

        //Z轴扫描总次数(Z轴进给次数)

        int Z_Scantimes = 0;

        if (z_interval == 0)
        {
            Z_Scantimes = 1;
        }
        else
        {
            Z_Scantimes = Convert.ToInt32(Math.Abs(z_end - z_start) / z_interval + 1);
        }

        //Jump从原点跳到Z的起点
        genLineDataNew(JUMP_SPEED, 0, 0, 0, 0, 0, 0, 0, 0, z_start, 0, 0);
        //JumpDelay
        genDelayData(0, 0, z_start, 0, 0, JUMP_DELAY, 0);
        
        double z_i = z_start;
        for (int j = 0; j < Z_Scantimes; j++)//Z轴进给循环
        {
            for(int k = 0; k < times; k++)
            {
                //Jump从原点跳到单层扫描起点
                genLineDataNew(JUMP_SPEED, 0, 0, 0, z_i, 0, 0, X_start, Y_start, z_i, A, B);
                //JumpDelay
                genDelayData(X_start, Y_start, z_i, A, B, JUMP_DELAY, 0);

                for (int i = 0; i < num / 2; i++)
                {
                double X_0 = X_start;
                double Y_0 = Y_start - i * interval * 2;
                genLineDataNew(speed, 1, X_0, Y_0, z_i, A, B, X_0 + length, Y_0, z_i, A, B);
                //MarkDelay
                genDelayData(X_0 + length, Y_0, z_i, A, B, MARK_DELAY, 0);
                genLineDataNew(speed, 0, X_0 + length, Y_0, z_i, A, B, X_0 + length, Y_0 - interval, z_i, A, B);              
                //JumpDelay
                genDelayData(X_0 + length, Y_0 - interval, z_i, A, B, JUMP_DELAY, 0);
                genLineDataNew(speed, 1, X_0 + length, Y_0 - interval, z_i, A, B, X_0, Y_0 - interval, z_i, A, B);
                //MarkDelay
                genDelayData(X_0, Y_0 - interval, z_i, A, B, MARK_DELAY, 0);
                }

                //Jump从单层扫描终点跳回单层扫描原点
                genLineDataNew(JUMP_SPEED, 0, X_start, Y_start - (num / 2 - 1) * interval * 2, z_i, A, B, 0, 0, z_i, 0, 0);
                //JumpDelay
                genDelayData(X_start, Y_start, z_i, A, B, JUMP_DELAY, 0);
            }
                z_i = z_start + (j + 1) * h * z_interval;
                //JumpZ进给
                genLineDataNew(JUMP_SPEED, 0, 0, 0, z_start + j * h * z_interval, 0, 0, 0, 0, z_i, 0, 0);
                //JumpDelay
                genDelayData(0, 0, z_i, 0, 0, JUMP_DELAY, 0);
        }

        genLineDataNew(speed, 0, X_start, Y_start - (num / 2 - 1) * interval * 2, z_end, A, B, 0, 0, 0, 0, 0);   
        DataBuffer.addProcessEnd();
    }






    public static void genLineDataNewly(double speed, int Laser_Switch, double X1, double Y1, double Z1, double X2, double Y2, double Z2)
    {
        speed *= 0.001;
        double length_xy = 0.001 * Math.Sqrt(Math.Pow(X2 - X1, 2) + Math.Pow(Y2 - Y1, 2)+ Math.Pow(Z2 - Z1, 2));
        double t = 0.00001; // 10 us

        int n_max = Convert.ToInt32(length_xy / (speed * t));
        if (length_xy != 0)
        {
            n_max = Convert.ToInt32((length_xy / (speed * t)) + 1);
        }

        if (X1 == X2 && Y1 == Y2 && Z1 == Z2)
        {
            correctionly(ref X1, ref Y1, ref Z1);
            DataBuffer.addProcessJumpData(Convert.ToUInt16(X1),Convert.ToUInt16(Y1), Convert.ToUInt16(Z1),0,0);
        }
        else
        {
            for (int i = 1; i <= n_max; i++)
            {
                double X = X1 + i * (X2 - X1) * speed * t / length_xy;
                double Y = Y1 + i * (Y2 - Y1) * speed * t / length_xy;
                double Z = Z1 + i * (Z2 - Z1) * speed * t / length_xy;
                correctionly(ref X, ref Y, ref Z);

                if (Laser_Switch == 1)
                {
                    if (i * 10 < LASER_ON_DELAY)
                    {
                        DataBuffer.addProcessJumpData(Convert.ToUInt16(X),
                                Convert.ToUInt16(Y), Convert.ToUInt16(Z),0,0);
                    }
                    else
                    {
                        DataBuffer.addProcessData(Convert.ToUInt16(X),
                                Convert.ToUInt16(Y), Convert.ToUInt16(Z),0,0);
                    }
                }
                else
                {
                    DataBuffer.addProcessJumpData(Convert.ToUInt16(X),
                                 Convert.ToUInt16(Y), Convert.ToUInt16(Z),0,0);
                }
            }
        }

    }

        public static void correctionly(ref double X, ref double Y, ref double Z)
    {

        // 增益
        double X_x = 773.895;   //系数校正773.895，778.062
        double Y_y = 778.062;
        double Z_z = 830;

        Z = Z - 4.5;

        double z_real = Z;     //倾角校正。和Z有关
        double X_z = -0.132943+0.002;//+显微镜视角下平面下移
        double Y_z = 0.1605982+0.001;//+显微镜视角下平面右移
        X_z *= z_real;
        Y_z *= z_real;

        X = (X+X_z) * X_x;
        Y = (Y+Y_z) * Y_y;
        Z = Z_z * Z;

        double degree = 44.4;//调整时，负数显微镜下顺时针//平面位移台44.4
        double tmp_radian = degree * Math.PI / 180;
        double temp_X = X;
        double temp_Y = Y;        

        X = temp_X * Math.Cos(tmp_radian) + temp_Y * Math.Sin(tmp_radian);      //旋转校正
        Y = temp_Y * Math.Cos(tmp_radian) - temp_X * Math.Sin(tmp_radian);


        X = X-Z+32768;
        Y += 32768;
        Z += 32768; 
    }



    public static void genCircleNewly(double X0, double Y0, double X1, double Y1,double Z1, double speed)
    {

        speed *= 0.001;

        double R;//计算圆半径
        R = Math.Sqrt((X1 - X0) * (X1 - X0) + (Y1 - Y0) * (Y1 - Y0));
        R = 0.001 * R;
        
        double t;//数据间隔时间10μs
        t = 0.00001;

        double T;
        T = 2 * Math.PI * R / speed;

        int n_max = Convert.ToInt32((T / t) + 1);
        int n = 0;

        while (n < n_max)
        {
            double X;
            double Y;
            double Z;

            X = X0 + R * Math.Cos((n * t * 360 / T) * 3.14 / 180) * 1000;//第n个数据点X坐标
            Y = Y0 + R * Math.Sin((n * t * 360 / T) * 3.14 / 180) * 1000;//第n个数据点Y坐标
            Z = Z1;
           
            correctionly(ref X, ref Y, ref Z);
            if (n * 10 < LASER_ON_DELAY)
            {

                DataBuffer.addProcessJumpData(Convert.ToUInt16(X),
                                Convert.ToUInt16(Y), Convert.ToUInt16(Z),
                                Convert.ToUInt16(0), Convert.ToUInt16(0));


            }
            else
            {
                DataBuffer.addProcessData(Convert.ToUInt16(X),
                                Convert.ToUInt16(Y), Convert.ToUInt16(Z),
                                Convert.ToUInt16(0), Convert.ToUInt16(0));
            }
            n = n + 1;
        }
    }

    public static void gen3DCircle_LineDataly_convex(double speed, int Laser_Switch, double X0, double Y0, double Z0, double R, double theta_1, double theta_2)
       
    {
        speed *= 0.001; //设置速度

        double radtheta_1 = theta_1 * Math.PI / 180;
        double radtheta_2 = theta_2 * Math.PI / 180;

        double length_circle = 0.001 * R * Math.Abs(radtheta_1-radtheta_2); //计算弧长 
        double t = 0.00001; // 10 us

        int n_max = Convert.ToInt32(length_circle / (speed * t));
        if (length_circle != 0)
        {
            n_max = Convert.ToInt32((length_circle / (speed * t)) + 1);
        }

        if (theta_1 == theta_2)
        {
            correctionly(ref X0, ref Y0, ref Z0);
            DataBuffer.addProcessJumpData(Convert.ToUInt16(X0),Convert.ToUInt16(Y0), Convert.ToUInt16(Z0),0,0);
        }
        else
        {
            for (int i = 1; i <= n_max; i++)
            {
                double X = X0 + R * Math.Sin(radtheta_1+(radtheta_2-radtheta_1)*i/n_max);
                // Log.Information(Convert.ToString(i));
                double Y = Y0;
                double Z = Z0 + R * Math.Cos(radtheta_1+(radtheta_2-radtheta_1)*i/n_max);

                correctionly(ref X, ref Y, ref Z);

                if (Laser_Switch == 1)
                {
                    if (i * 10 < LASER_ON_DELAY)
                    {
                        DataBuffer.addProcessJumpData(Convert.ToUInt16(X),
                                Convert.ToUInt16(Y), Convert.ToUInt16(Z),0,0);
                    }
                    else
                    {
                        DataBuffer.addProcessData(Convert.ToUInt16(X),
                                Convert.ToUInt16(Y), Convert.ToUInt16(Z),0,0);
                    }
                }
                else
                {
                    DataBuffer.addProcessJumpData(Convert.ToUInt16(X),
                                 Convert.ToUInt16(Y), Convert.ToUInt16(Z),0,0);
                }
            }
        }

    }



    public static void gen3DCircle_LineDataly_convex_filled(double speed, double X0, double Y0, double Z0, double R, double theta_1, double theta_2, double width_Y, double interval_Y)
    {
        double n = width_Y / interval_Y;
        double radtheta_1 = theta_1 * Math.PI / 180;
        double radtheta_2 = theta_2 * Math.PI / 180;

        double X_Start1 = X0 + R * Math.Sin(radtheta_1);
        double Y_Start1 = Y0 - width_Y/2;
        double Z_Start1 = Z0 + R * Math.Cos(radtheta_1);

        double X_End1 = X0 + R * Math.Sin(radtheta_2);
        double Y_End1 = Y0 - width_Y/2;
        double Z_End1 = Z0 + R * Math.Cos(radtheta_2);
        //不出光Jump至起点
        genLineDataNewly(JUMP_SPEED, 0, 0, 0, 0, X_Start1, Y_Start1, Z_Start1);
        //JumpDelay
        genDelayData(X_Start1, Y_Start1, Z_Start1, 0, 0, JUMP_DELAY, 0);


        for (int i = 0; i < n/2; i++)
        {
            gen3DCircle_LineDataly_convex(speed,1,X0,Y_Start1+width_Y*(2*i)/n,Z0,R,theta_1,theta_2);
            genDelayData(X_End1,Y_End1+width_Y*(2*i)/n,Z_End1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(JUMP_SPEED, 0,X_End1,Y_End1+width_Y*(2*i)/n,Z_End1, X_End1,Y_End1+width_Y*(2*i+1)/n,Z_End1);
            genDelayData(X_End1,Y_End1+width_Y*(2*i+1)/n,Z_End1, 0, 0, JUMP_DELAY, 0);

            gen3DCircle_LineDataly_convex(speed,1,X0,Y_Start1+width_Y*(2*i+1)/n,Z0,R,theta_2,theta_1);
            genDelayData(X_Start1,Y_Start1+width_Y*(2*i+1)/n,Z_Start1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(JUMP_SPEED, 0,X_Start1,Y_Start1+width_Y*(2*i+1)/n,Z_Start1, X_Start1,Y_Start1+width_Y*(2*i+2)/n,Z_Start1);
            genDelayData(X_Start1,Y_Start1+width_Y*(2*i+2)/n,Z_Start1, 0, 0, JUMP_DELAY, 0);
        }

    }
    public static void genCircle_filledly(double X0, double Y0, double X1, double Y1, double Z1, double speed, double interval)
    {

        double R;//计算圆半径
        R = Math.Sqrt((X1 - X0) * (X1 - X0) + (Y1 - Y0) * (Y1 - Y0));
        double numcircle = Convert.ToInt32(R / interval + 1);
        

        genLineDataNewly(JUMP_SPEED, 0, 0, 0, 0, X0, Y0, Z1);
        //JumpDelay
        genDelayData(X0, Y0, Z1, 0, 0, JUMP_DELAY, 0);
        
        double X_circle = X1;
        double Y_circle = X1;

        for (int i = 1; i <= numcircle; i++)
        {
            genCircleNewly(X0, Y0, (i/numcircle) * X_circle, (i/numcircle) * Y_circle, Z1, speed);
            genLineDataNewly(JUMP_SPEED, 0, X0+(i/numcircle)*R, Y0, Z1, X0+((i+1)/numcircle)*R, Y0, Z1);
            genDelayData(X0+((i+1)/numcircle)*R, Y0, Z1, 0, 0, JUMP_DELAY, 0);

        }
    }

        //填充同心圆
    public static void genCircleFilledDataly(double X0, double Y0, double X1, double Y1, double z_start, double speed, double r_min, double r_interval)
    
    {
        //数据生成准备c
        double r_max = Math.Sqrt(Math.Pow(X1 - X0, 2) + Math.Pow(Y1 - Y0, 2));
        int circle_num = Convert.ToInt32((r_max - r_min) / r_interval + 1);
        double r_min_real = r_max - (circle_num - 1) * r_interval;
        double Z = z_start;//定义初始Z轴坐标

        double X = X0 + r_min_real;
        double Y = Y0;


        //Jump从原点跳到第一个圆的起点
        //genLineDataNewly(double speed, int Laser_Switch, double X1, double Y1, double Z1, double X2, double Y2, double Z2)
        genLineDataNewly(JUMP_SPEED, 0, 0, 0, Z, X, Y, Z);
        //JumpDelay
        genDelayData(X, Y, Z, 0, 0, JUMP_DELAY, 0);

        //主循环-由内到外
        int i = 0;
        while (i < circle_num)//主循环-由内到外
        {
            //MarkCircle
            //public static void genCircleNewly(double X0, double Y0, double X1, double Y1,double Z1, double speed)
            genCircleNewly(X0, Y0, X, Y, Z, speed);
            //MarkDelay
            genDelayData(X, Y, Z, 0, 0, MARK_DELAY, LASER_OFF_DELAY);
            i = i + 1;

            double X_Temp = X;

            X = X + r_interval;//子循环X坐标变化

            if (i == circle_num)//如果所有循环扫描完成，则跳转回原点，并退出循环
            {
                //Jump
                genLineDataNewly(JUMP_SPEED, 0, X_Temp, Y, Z, 0, 0, Z);
                break;
            }
            else//否则跳转到下一个圆的起点
                //Jump
                genLineDataNewly(JUMP_SPEED, 0, X_Temp, Y, Z, X, Y, Z);
            //JumpDelay
            genDelayData(X, Y, Z, 0, 0, JUMP_DELAY, 0);
        }
    }

    public static void genRectangleData3D(double X0, double Y0, double Z0, double X1, double Y1, double Z1, double speed, double Y_interval)//回型填充
    {
        double Y_length = Math.Abs(2 * (Y1 - Y0));//Y方向扫描总长度
        double X_length = Math.Abs(2 * (X1 - X0));//X方向扫描总长度
        double Z_length = Math.Abs(2 * (Z1 - Z0));//Z方向扫描总长度

        double Y_Start = Y0 - Y_length/2;
        double X_Start = X0 - X_length/2;
        double Z_Start = Z0 - Z_length/2;

        double X_interval = X_length*Y_interval/Y_length;
        double Z_interval = Z_length*Y_interval/Y_length;
        double num = Y_length/Y_interval;
        //public static void genLineDataNewly(double speed, int Laser_Switch, double X1, double Y1, double Z1, double X2, double Y2, double Z2)
        //不出光Jump至起点
        genLineDataNewly(JUMP_SPEED, 0, 0, 0, 0, X_Start, Y_Start, Z_Start);
        //JumpDelay
        genDelayData(X_Start, Y_Start, Z_Start, 0, 0, JUMP_DELAY, 0);
        for (int i = 0; i < num/4; i++)
        {
            genLineDataNewly(speed, 1, X_Start+i*X_interval, Y_Start+i*Y_interval, Z_Start+i*Z_interval, X_Start+i*X_interval, Y1-i*Y_interval, Z_Start+i*Z_interval);
            genDelayData(X_Start+i*X_interval, Y1-i*Y_interval, Z_Start+i*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(speed, 1, X_Start+i*X_interval, Y1-i*Y_interval, Z_Start+i*Z_interval, X1-i*X_interval, Y1-i*Y_interval, Z1-i*Z_interval);
            genDelayData(X1-i*X_interval, Y1-i*Y_interval, Z1-i*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(speed, 1, X1-i*X_interval, Y1-i*Y_interval, Z1-i*Z_interval, X1-i*X_interval, Y_Start+i*Y_interval, Z1-i*Z_interval);
            genDelayData(X1-i*X_interval, Y_Start+i*Y_interval, Z1-i*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(speed, 1, X1-i*X_interval, Y_Start+i*Y_interval, Z1-i*Z_interval, X_Start+i*X_interval, Y_Start+i*Y_interval, Z_Start+i*Z_interval);
            genDelayData(X_Start+i*X_interval, Y_Start+i*Y_interval, Z_Start+i*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(JUMP_SPEED, 0, X_Start+i*X_interval, Y_Start+i*Y_interval, Z_Start+i*Z_interval, X_Start+(i+1)*X_interval, Y_Start+(i+1)*Y_interval, Z_Start+(i+1)*Z_interval);
            genDelayData(X_Start+(i+1)*X_interval, Y_Start+(i+1)*Y_interval, Z_Start+(i+1)*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
        }

    }


    public static void genFilledRectangleData3D_Y(double X0, double Y0, double Z0, double X1, double Y1, double Z1, double speed, double Y_interval )
    //(X0,Y0,Z0)为中心，(X1,Y1,Z1)为右上角点,X方向扫描，X、Z升高，间隔为Y方向
    {
        
        double Y_length = Math.Abs(2 * (Y1 - Y0));//Y方向扫描总长度
        double X_length = Math.Abs(2 * (X1 - X0));//X方向扫描总长度
        double Z_length = Math.Abs(2 * (Z1 - Z0));//Z方向扫描总长度

        double Y_Start = Y0 - Y_length/2;
        double X_Start = X0 - X_length/2;
        double Z_Start = Z0 - Z_length/2;

        double num = (Y_length/Y_interval);

        //public static void genLineDataNewly(double speed, int Laser_Switch, double X1, double Y1, double Z1, double X2, double Y2, double Z2)
        //不出光Jump至起点
        genLineDataNewly(JUMP_SPEED, 0, 0, 0, 0, X_Start, Y_Start, Z_Start);
        //JumpDelay
        genDelayData(X_Start, Y_Start, Z_Start, 0, 0, JUMP_DELAY, 0);

        for (int i = 0; i < num/2; i++)
        {
            genLineDataNewly(speed, 1, X_Start, Y_Start+2*i*Y_interval, Z_Start, X1, Y_Start+2*i*Y_interval, Z1);
            genDelayData(X1, Y_Start+2*i*Y_interval, Z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(JUMP_SPEED, 0, X1, Y_Start+2*i*Y_interval, Z1, X1, Y_Start+(2*i+1)*Y_interval, Z1);
            genDelayData(X1, Y_Start+(2*i+1)*Y_interval, Z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(speed, 1, X1, Y_Start+(2*i+1)*Y_interval, Z1, X_Start, Y_Start+(2*i+1)*Y_interval, Z_Start);
            genDelayData(X_Start, Y_Start+(2*i+1)*Y_interval, Z_Start, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(JUMP_SPEED, 0, X_Start, Y_Start+(2*i+1)*Y_interval, Z_Start, X_Start, Y_Start+(2*i+2)*Y_interval, Z_Start);
            genDelayData(X_Start, Y_Start+(2*i+2)*Y_interval, Z_Start, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
        }
            double k = num/2;  //最后一条不出光
            genLineDataNewly(speed, 1, X_Start, Y_Start+2*k*Y_interval, Z_Start, X1, Y_Start+2*k*Y_interval, Z1);
            genDelayData(X1, Y_Start+2*k*Y_interval, Z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(JUMP_SPEED, 0, X1, Y_Start+2*k*Y_interval, Z1, X1, Y_Start+(2*k+1)*Y_interval, Z1);
            genDelayData(X1, Y_Start+(2*k+1)*Y_interval, Z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(speed, 0, X1, Y_Start+(2*k+1)*Y_interval, Z1, X_Start, Y_Start+(2*k+1)*Y_interval, Z_Start);
            genDelayData(X_Start, Y_Start+(2*k+1)*Y_interval, Z_Start, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(JUMP_SPEED, 0, X_Start, Y_Start+(2*k+1)*Y_interval, Z_Start, X_Start, Y_Start+(2*k+2)*Y_interval, Z_Start);
            genDelayData(X_Start, Y_Start+(2*k+2)*Y_interval, Z_Start, 0, 0, POLYGON_DELAY, POLYGON_DELAY);

    }

        public static void genFilledRectangleData3D_Y2(double X0, double Y0, double Z0, double X1, double Y1, double Z1, double speed, double X_interval )
    //(X0,Y0,Z0)为中心，(X1,Y1,Z1)为右上角点，Y方向扫描，X、Z升高，间隔为X方向
    //该程序与genFilledRectangleData3D_Y配合，形成填充实验
    {
        
        double Y_length = Math.Abs(2 * (Y1 - Y0));//Y方向扫描总长度
        double X_length = Math.Abs(2 * (X1 - X0));//X方向扫描总长度
        double Z_length = Math.Abs(2 * (Z1 - Z0));//Z方向扫描总长度

        double Y_Start = Y0 - Y_length/2;
        double X_Start = X0 - X_length/2;
        double Z_Start = Z0 - Z_length/2;

        double num = (X_length/X_interval);
        double Z_interval = X_interval*Z_length/X_length;

        //public static void genLineDataNewly(double speed, int Laser_Switch, double X1, double Y1, double Z1, double X2, double Y2, double Z2)
        //不出光Jump至起点
        genLineDataNewly(JUMP_SPEED, 0, 0, 0, 0, X_Start, Y_Start, Z_Start);
        //JumpDelay
        genDelayData(X_Start, Y_Start, Z_Start, 0, 0, JUMP_DELAY, 0);

        for (int i = 0; i < num/2; i++)
        {
            genLineDataNewly(speed, 1, X_Start+2*i*X_interval, Y_Start, Z_Start+2*i*Z_interval, X_Start+2*i*X_interval, Y1, Z_Start+2*i*Z_interval);
            genDelayData(X_Start+2*i*X_interval, Y1, Z_Start+2*i*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(JUMP_SPEED, 0, X_Start+2*i*X_interval, Y1, Z_Start+2*i*Z_interval, X_Start+(2*i+1)*X_interval, Y1, Z_Start+(2*i+1)*Z_interval);
            genDelayData(X_Start+(2*i+1)*X_interval, Y1, Z_Start+(2*i+1)*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(speed, 1, X_Start+(2*i+1)*X_interval, Y1, Z_Start+(2*i+1)*Z_interval, X_Start+(2*i+1)*X_interval, Y_Start, Z_Start+(2*i+1)*Z_interval);
            genDelayData(X_Start+(2*i+1)*X_interval, Y_Start, Z_Start+(2*i+1)*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(JUMP_SPEED, 0, X_Start+(2*i+1)*X_interval, Y_Start, Z_Start+(2*i+1)*Z_interval, X_Start+(2*i+2)*X_interval, Y_Start, Z_Start+(2*i+2)*Z_interval);
            genDelayData(X_Start+(2*i+2)*X_interval, Y_Start, Z_Start+(2*i+2)*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
        }
            double k = num/2; //最后一条线不出光
            genLineDataNewly(speed, 1, X_Start+2*k*X_interval, Y_Start, Z_Start+2*k*Z_interval, X_Start+2*k*X_interval, Y1, Z_Start+2*k*Z_interval);
            genDelayData(X_Start+2*k*X_interval, Y1, Z_Start+2*k*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(JUMP_SPEED, 0, X_Start+2*k*X_interval, Y1, Z_Start+2*k*Z_interval, X_Start+(2*k+1)*X_interval, Y1, Z_Start+(2*k+1)*Z_interval);
            genDelayData(X_Start+(2*k+1)*X_interval, Y1, Z_Start+(2*k+1)*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(speed, 0, X_Start+(2*k+1)*X_interval, Y1, Z_Start+(2*k+1)*Z_interval, X_Start+(2*k+1)*X_interval, Y_Start, Z_Start+(2*k+1)*Z_interval);
            genDelayData(X_Start+(2*k+1)*X_interval, Y_Start, Z_Start+(2*k+1)*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(JUMP_SPEED, 0, X_Start+(2*k+1)*X_interval, Y_Start, Z_Start+(2*k+1)*Z_interval, X_Start+(2*k+2)*X_interval, Y_Start, Z_Start+(2*k+2)*Z_interval);
            genDelayData(X_Start+(2*k+2)*X_interval, Y_Start, Z_Start+(2*k+2)*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);

    }
    
       public static void genFilledRectangleData3D_X(double X0, double Y0, double Z0, double X1, double Y1, double Z1, double speed, double X_interval )
    //(X0,Y0,Z0)为中心，(X1,Y1,Z1)为右上角点，Y方向扫描，Y、Z升高，间隔为X方向
    {

        double X_length = Math.Abs(2 * (X1 - X0));//Y方向扫描总长度
        double Y_length = Math.Abs(2 * (Y1 - Y0));//X方向扫描总长度
        double Z_length = Math.Abs(2 * (Z1 - Z0));//Z方向扫描总长度

        double X_Start = X0 - X_length/2;
        double Y_Start = Y0 - Y_length/2;
        double Z_Start = Z0 - Z_length/2;

        double num = (X_length/X_interval);

        //public static void genLineDataNewly(double speed, int Laser_Switch, double X1, double Y1, double Z1, double X2, double Y2, double Z2)
        //不出光Jump至起点
        genLineDataNewly(JUMP_SPEED, 0, 0, 0, 0, X_Start, Y_Start, Z_Start);
        //JumpDelay
        genDelayData(X_Start, Y_Start, Z_Start, 0, 0, JUMP_DELAY, 0);
    

        for (int i = 0; i < num/2; i++)
        {
            genLineDataNewly(speed, 1, X_Start+2*i*X_interval, Y_Start, Z_Start, X_Start+2*i*X_interval, Y1, Z1);
            genDelayData(X_Start+2*i*X_interval, Y1, Z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(JUMP_SPEED, 0, X_Start+2*i*X_interval, Y1, Z1, X_Start+(2*i+1)*X_interval, Y1, Z1);
            genDelayData(X_Start+(2*i+1)*X_interval, Y1, Z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(speed, 1,X_Start+(2*i+1)*X_interval, Y1, Z1, X_Start+(2*i+1)*X_interval, Y_Start, Z_Start);
            genDelayData(X_Start+(2*i+1)*X_interval, Y_Start, Z_Start, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(JUMP_SPEED, 0, X_Start+(2*i+1)*X_interval, Y_Start, Z_Start, X_Start+(2*i+2)*X_interval, Y_Start, Z_Start);
            genDelayData(X_Start+(2*i+2)*X_interval, Y_Start, Z_Start, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
        }
            double k = num/2; //最后一条线不出光
            genLineDataNewly(speed, 1, X_Start+2*k*X_interval, Y_Start, Z_Start, X_Start+2*k*X_interval, Y1, Z1);
            genDelayData(X_Start+2*k*X_interval, Y1, Z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(JUMP_SPEED, 0, X_Start+2*k*X_interval, Y1, Z1, X_Start+(2*k+1)*X_interval, Y1, Z1);
            genDelayData(X_Start+(2*k+1)*X_interval, Y1, Z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(speed, 0,X_Start+(2*k+1)*X_interval, Y1, Z1, X_Start+(2*k+1)*X_interval, Y_Start, Z_Start);
            genDelayData(X_Start+(2*k+1)*X_interval, Y_Start, Z_Start, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(JUMP_SPEED, 0, X_Start+(2*k+1)*X_interval, Y_Start, Z_Start, X_Start+(2*k+2)*X_interval, Y_Start, Z_Start);
            genDelayData(X_Start+(2*k+2)*X_interval, Y_Start, Z_Start, 0, 0, POLYGON_DELAY, POLYGON_DELAY);

    }

            public static void genFilledRectangleData3D_X2(double X0, double Y0, double Z0, double X1, double Y1, double Z1, double speed, double Y_interval )
    //(X0,Y0,Z0)为中心，(X1,Y1,Z1)为右上角点，X方向扫描，Y、Z升高，间隔为Y方向
    //该程序与genFilledRectangleData3D_X配合，形成填充实验
    {

        double X_length = Math.Abs(2 * (X1 - X0));//X方向扫描总长度
        double Y_length = Math.Abs(2 * (Y1 - Y0));//Y方向扫描总长度
        double Z_length = Math.Abs(2 * (Z1 - Z0));//Z方向扫描总长度

        double X_Start = X0 - X_length/2;
        double Y_Start = Y0 - Y_length/2;
        double Z_Start = Z0 - Z_length/2;


        double num = (Y_length/Y_interval);
        double Z_interval = Y_interval*Z_length/Y_length;

        //public static void genLineDataNewly(double speed, int Laser_Switch, double X1, double Y1, double Z1, double X2, double Y2, double Z2)
        //不出光Jump至起点
        genLineDataNewly(JUMP_SPEED, 0, 0, 0, 0, X_Start, Y_Start, Z_Start);
        //JumpDelay
        genDelayData(X_Start, Y_Start, Z_Start, 0, 0, JUMP_DELAY, 0);

        for (int i = 0; i < num/2; i++)
        {
            genLineDataNewly(speed, 1, X_Start, Y_Start+2*i*Y_interval, Z_Start+2*i*Z_interval, X1, Y_Start+2*i*Y_interval, Z_Start+2*i*Z_interval);
            genDelayData(X1, Y_Start+2*i*Y_interval, Z_Start+2*i*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(JUMP_SPEED, 0, X1, Y_Start+2*i*Y_interval, Z_Start+2*i*Z_interval, X1, Y_Start+(2*i+1)*Y_interval, Z_Start+(2*i+1)*Z_interval);
            genDelayData(X1, Y_Start+(2*i+1)*Y_interval, Z_Start+(2*i+1)*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(speed, 1, X1, Y_Start+(2*i+1)*Y_interval, Z_Start+(2*i+1)*Z_interval, X_Start, Y_Start+(2*i+1)*Y_interval, Z_Start+(2*i+1)*Z_interval);
            genDelayData(X_Start, Y_Start+(2*i+1)*Y_interval, Z_Start+(2*i+1)*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(JUMP_SPEED, 0, X_Start, Y_Start+(2*i+1)*Y_interval, Z_Start+(2*i+1)*Z_interval, X_Start, Y_Start+(2*i+2)*Y_interval, Z_Start+(2*i+2)*Z_interval);
            genDelayData(X_Start, Y_Start+(2*i+2)*Y_interval, Z_Start+(2*i+2)*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
        }
            double k = num/2; //最后一条线不出光
            genLineDataNewly(speed, 1, X_Start, Y_Start+2*k*Y_interval, Z_Start+2*k*Z_interval, X1, Y_Start+2*k*Y_interval, Z_Start+2*k*Z_interval);
            genDelayData(X1, Y_Start+2*k*Y_interval, Z_Start+2*k*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(JUMP_SPEED, 0, X1, Y_Start+2*k*Y_interval, Z_Start+2*k*Z_interval, X1, Y_Start+(2*k+1)*Y_interval, Z_Start+(2*k+1)*Z_interval);
            genDelayData(X1, Y_Start+(2*k+1)*Y_interval, Z_Start+(2*k+1)*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(speed, 0, X1, Y_Start+(2*k+1)*Y_interval, Z_Start+(2*k+1)*Z_interval, X_Start, Y_Start+(2*k+1)*Y_interval, Z_Start+(2*k+1)*Z_interval);
            genDelayData(X_Start, Y_Start+(2*k+1)*Y_interval, Z_Start+(2*k+1)*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(JUMP_SPEED, 0, X_Start, Y_Start+(2*k+1)*Y_interval, Z_Start+(2*k+1)*Z_interval, X_Start, Y_Start+(2*k+2)*Y_interval, Z_Start+(2*k+2)*Z_interval);
            genDelayData(X_Start, Y_Start+(2*k+2)*Y_interval, Z_Start+(2*k+2)*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);


    }


  public static void genFilledRectangleData3D_Y_and_Y2(double X0, double Y0, double Z0, double X1, double Y1, double Z1, double speed, double interval,int times = 1 )
    //(X0,Y0,Z0)为中心，(X1,Y1,Z1)为右上角点
    {
        double num_Loop = 50;
        for (int i = 0; i <= times; i++)
        {
            //genFilledRectangleData3D_Y2(X0, Y0, Z0, X1, Y1, Z1, speed, interval );
            //genFilledRectangleData3D_Y(X0, Y0, Z0, X1, Y1, Z1, speed, interval );
            genFilledRectangleData3D_till(X0, Y0, Z0, X1, Y1, Z1, speed, interval );
        }

    }
    public static void genFilledRectangleData3D_X_and_X2(double X0, double Y0, double Z0, double X1, double Y1, double Z1, double speed, double interval)
    //(X0,Y0,Z0)为中心，(X1,Y1,Z1)为右上角点
    {
        double num_Loop = 10;
        for (int i = 0; i <= num_Loop; i++)
        {
            genFilledRectangleData3D_X(X0, Y0, Z0, X1, Y1, Z1, speed, interval );
            genFilledRectangleData3D_X2(X0, Y0, Z0, X1, Y1, Z1, speed, interval );
        }

    }

    public static void genFilledRectangleData3D_theta(double X0, double Y0, double Z0, double X1, double Y1, double Z1, double speed, double interval, double theta)
    //(X0,Y0,Z0)为中心，(X1,Y1,Z1)为右上角点。X,Z升高
    {
        
        double Y_length = Math.Abs(2 * (Y1 - Y0));//Y方向扫描总长度
        double X_length = Math.Abs(2 * (X1 - X0));//X方向扫描总长度
        double Z_length = Math.Abs(2 * (Z1 - Z0));//Z方向扫描总长度

        double Y_Start = Y0 - Y_length/2;
        double X_Start = X0 - X_length/2;
        double Z_Start = Z0 - Z_length/2;

        double theta_radian = theta * Math.PI / 180;

        double Y_interval = interval / Math.Sin(theta_radian);
        double X_interval = interval / Math.Cos(theta_radian) * X_length / Math.Sqrt(Math.Pow(X_length, 2) + Math.Pow(Z_length, 2));
        double Z_interval = interval / Math.Cos(theta_radian) * Z_length / Math.Sqrt(Math.Pow(X_length, 2) + Math.Pow(Z_length, 2));

        //public static void genLineDataNewly(double speed, int Laser_Switch, double X1, double Y1, double Z1, double X2, double Y2, double Z2)
        //不出光Jump至起点
        genLineDataNewly(JUMP_SPEED, 0, 0, 0, 0, X_Start, Y_Start, Z_Start);
        //JumpDelay
        genDelayData(X_Start, Y_Start, Z_Start, 0, 0, JUMP_DELAY, 0);

        double num=Math.Max(Y_length/Y_interval,X_length/X_interval);
        num=Math.Max(num,Z_length/Z_interval);
        
        for (int i = 0; i <= num/2; i++)
        {
            genLineDataNewly(JUMP_SPEED, 0, X_Start, Math.Min(Y_Start+2*i*Y_interval, Y1), Z_Start, X_Start, Math.Min(Y_Start+(2*i+1)*Y_interval,Y1), Z_Start);
            genDelayData(X_Start, Math.Min(Y_Start+2*i*Y_interval, Y1), Z_Start, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(speed, 1, X_Start, Math.Min(Y_Start+2*i*Y_interval, Y1), Z_Start, Math.Min(X_Start+(2*i+1)*X_interval,X1), Y_Start, Math.Min(Z_Start+(2*i+1)*Z_interval,Z1));
            genDelayData(Math.Min(X_Start+(2*i+1)*X_interval,X1), Y_Start, Math.Min(Z_Start+(2*i+1)*Z_interval,Z1), 0, 0, POLYGON_DELAY, POLYGON_DELAY);


            genLineDataNewly(JUMP_SPEED, 0, X_Start+(2*i+1)*X_interval, Y_Start, Z_Start+(2*i+1)*Z_interval, X_Start+(2*i+2)*X_interval, Y_Start, Z_Start+(2*i+2)*Z_interval);
            genDelayData(X_Start+(2*i+2)*X_interval, Y_Start, Z_Start+(2*i+2)*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(speed, 1, X_Start+(2*i+2)*X_interval, Y_Start, Z_Start+(2*i+2)*Z_interval, X_Start, Y_Start+(2*i+2)*Y_interval, Z_Start);
            genDelayData(X_Start, Y_Start+(2*i+2)*Y_interval, Z_Start, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
        }

        for (int i = 0; i <= num/2; i++)
        {
            genLineDataNewly(JUMP_SPEED, 0, X_Start+2*i*X_interval, Y1, Z_Start+2*i*Z_interval, X_Start+(2*i+1)*X_interval, Y1, Z_Start+(2*i+1)*Z_interval);
            genDelayData(X_Start+(2*i+1)*X_interval, Y1, Z_Start+(2*i+1)*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(speed, 1, X_Start+(2*i+1)*X_interval, Y1, Z_Start+(2*i+1)*Z_interval, X1, Y_Start+(2*i+1)*Y_interval, Z1);
            genDelayData(X1, Y_Start+(2*i+1)*Y_interval, Z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(JUMP_SPEED, 0, X1, Y_Start+(2*i+1)*Y_interval, Z1, X1, Y_Start+(2*i+2)*Y_interval, Z1);
            genDelayData(X1, Y_Start+(2*i+2)*Y_interval, Z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(speed, 1, X1, Y_Start+(2*i+2)*Y_interval, Z1, X_Start+(2*i+2)*X_interval, Y1, Z_Start+(2*i+2)*Z_interval);
            genDelayData(X_Start+(2*i+2)*X_interval, Y1, Z_Start+(2*i+2)*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
        }

    }

    public static void genFilledRectangleData3D_till(double X0, double Y0, double Z0, double X1, double Y1, double Z1, double speed, double Z_interval )
    //(X0,Y0,Z0)为中心，(X1,Y1,Z1)为右上角点。X,Z升高
    {
        
        double Y_length = Math.Abs(2 * (Y1 - Y0));//Y方向扫描总长度
        double X_length = Math.Abs(2 * (X1 - X0));//X方向扫描总长度
        double Z_length = Math.Abs(2 * (Z1 - Z0));//Z方向扫描总长度

        double Y_Start = Y0 - Y_length/2;
        double X_Start = X0 - X_length/2;
        double Z_Start = Z0 - Z_length/2;

        double num = Z_length/Z_interval;
        double X_interval = Z_interval*X_length/Z_length;
        double Y_interval = Z_interval*Y_length/Z_length;

        //public static void genLineDataNewly(double speed, int Laser_Switch, double X1, double Y1, double Z1, double X2, double Y2, double Z2)
        //不出光Jump至起点
        genLineDataNewly(JUMP_SPEED, 0, 0, 0, 0, X_Start, Y_Start, Z_Start);
        //JumpDelay
        genDelayData(X_Start, Y_Start, Z_Start, 0, 0, JUMP_DELAY, 0);

        for (int i = 0; i <= num/2; i++)
        {
            genLineDataNewly(JUMP_SPEED, 0, X_Start, Y_Start+2*i*Y_interval, Z_Start, X_Start, Y_Start+(2*i+1)*Y_interval, Z_Start);
            genDelayData(X_Start, Y_Start+(2*i+1)*Y_interval, Z_Start, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(speed, 1, X_Start, Y_Start+(2*i+1)*Y_interval, Z_Start, X_Start+(2*i+1)*X_interval, Y_Start, Z_Start+(2*i+1)*Z_interval);
            genDelayData(X_Start+(2*i+1)*X_interval, Y_Start, Z_Start+(2*i+1)*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(JUMP_SPEED, 0, X_Start+(2*i+1)*X_interval, Y_Start, Z_Start+(2*i+1)*Z_interval, X_Start+(2*i+2)*X_interval, Y_Start, Z_Start+(2*i+2)*Z_interval);
            genDelayData(X_Start+(2*i+2)*X_interval, Y_Start, Z_Start+(2*i+2)*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(speed, 1, X_Start+(2*i+2)*X_interval, Y_Start, Z_Start+(2*i+2)*Z_interval, X_Start, Y_Start+(2*i+2)*Y_interval, Z_Start);
            genDelayData(X_Start, Y_Start+(2*i+2)*Y_interval, Z_Start, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
        }

        for (int i = 0; i <= num/2; i++)
        {
            genLineDataNewly(JUMP_SPEED, 0, X_Start+2*i*X_interval, Y1, Z_Start+2*i*Z_interval, X_Start+(2*i+1)*X_interval, Y1, Z_Start+(2*i+1)*Z_interval);
            genDelayData(X_Start+(2*i+1)*X_interval, Y1, Z_Start+(2*i+1)*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(speed, 1, X_Start+(2*i+1)*X_interval, Y1, Z_Start+(2*i+1)*Z_interval, X1, Y_Start+(2*i+1)*Y_interval, Z1);
            genDelayData(X1, Y_Start+(2*i+1)*Y_interval, Z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(JUMP_SPEED, 0, X1, Y_Start+(2*i+1)*Y_interval, Z1, X1, Y_Start+(2*i+2)*Y_interval, Z1);
            genDelayData(X1, Y_Start+(2*i+2)*Y_interval, Z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(speed, 1, X1, Y_Start+(2*i+2)*Y_interval, Z1, X_Start+(2*i+2)*X_interval, Y1, Z_Start+(2*i+2)*Z_interval);
            genDelayData(X_Start+(2*i+2)*X_interval, Y1, Z_Start+(2*i+2)*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
        }

    }

           public static void genFilledRectangleData3D_Yord(double X0, double Y0, double Z0, double X1, double Y1, double Z1, double speed, double Z_interval )
    //(X0,Y0,Z0)为中心，(X1,Y1,Z1)为右上角点，Y方向扫描，X、Z升高，间隔为X方向
    //作为透射式扫描方式，和genFilledRectangleData3D_Y_and_Y2相对比
    {

        double Z_length = Math.Abs(2 * (Z1 - Z0));//Z方向扫描总长度
        double X_length = Math.Abs(2 * (X1 - X0));//Y方向扫描总长度
        double Y_length = Math.Abs(2 * (Y1 - Y0));//X方向扫描总长度

        double Z_Start = Z0 - Z_length/2;
        double X_Start = X0 - X_length/2;
        double Y_Start = Y0 - Y_length/2;
        
        double X_interval = Z_interval * X_length/Z_length;

        double num = Z_length/Z_interval;

        //public static void genLineDataNewly(double speed, int Laser_Switch, double X1, double Y1, double Z1, double X2, double Y2, double Z2)
        //不出光Jump至起点
        genLineDataNewly(JUMP_SPEED, 0, 0, 0, 0, X_Start, Y_Start, Z_Start);
        //JumpDelay
        genDelayData(X_Start, Y_Start, Z_Start, 0, 0, JUMP_DELAY, 0);

        for (int i = 0; i <= num/2; i++)
        {
            genLineDataNewly(speed, 1, X_Start+2*i*X_interval, Y_Start, Z_Start+2*i*Z_interval, X_Start+2*i*X_interval, Y1, Z_Start+2*i*Z_interval);
            genDelayData(X_Start+2*i*X_interval, Y1, Z_Start+2*i*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(JUMP_SPEED, 0, X_Start+2*i*X_interval, Y1, Z_Start+2*i*Z_interval, X_Start+(2*i+1)*X_interval, Y1, Z_Start+(2*i+1)*Z_interval);
            genDelayData(X_Start+(2*i+1)*X_interval, Y1, Z_Start+(2*i+1)*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(speed, 1, X_Start+(2*i+1)*X_interval, Y1, Z_Start+(2*i+1)*Z_interval, X_Start+(2*i+1)*X_interval, Y_Start, Z_Start+(2*i+1)*Z_interval);
            genDelayData(X_Start+(2*i+1)*X_interval, Y_Start, Z_Start+(2*i+1)*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            genLineDataNewly(JUMP_SPEED, 0, X_Start+(2*i+1)*X_interval, Y_Start, Z_Start+(2*i+1)*Z_interval, X_Start+(2*i+2)*X_interval, Y_Start, Z_Start+(2*i+2)*Z_interval);
            genDelayData(X_Start+(2*i+2)*X_interval, Y_Start, Z_Start+(2*i+2)*Z_interval, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
        }

    }

    public static void genFilledRectangleData3D_beta(double X0, double Y0, double Z0, double X1, double Y1, double Z1, double speed, double interval, double beta)
    //(X0,Y0,Z0)为中心，(X1,Y1,Z1)为右上角点。X,Z升高，interval为线间隔
    {
        
        double Y_length = Math.Abs(2 * (Y1 - Y0));//Y方向扫描总长度
        double X_length = Math.Abs(2 * (X1 - X0));//X方向扫描总长度
        double Z_length = Math.Abs(2 * (Z1 - Z0));//Z方向扫描总长度

        double Y_Start = Y0 - Y_length/2;
        double X_Start = X0 - X_length/2;
        double Z_Start = Z0 - Z_length/2;

        double Y_LimL = Y_Start;         //左上角点坐标
        double X_LimL = X_Start + X_length;
        double Z_LimL = Z_Start + Z_length;

        double Y_LimR = Y_Start + Y_length;     //右下角点坐标
        double X_LimR = X_Start;
        double Z_LimR = Z_Start;

        double alpha =  Math.Atan(Z_length/X_length);

        if (beta == 0)  //横扫
        {
            double delta_x1 = interval * Math.Cos(alpha);
            double delta_z1 = interval * Math.Sin(alpha);
            double num1 = X_length / delta_x1;
            genLineDataNewly(JUMP_SPEED, 0, 0, 0, 0, X_Start, Y_Start, Z_Start);
            //JumpDelay
            genDelayData(X_Start, Y_Start, Z_Start, 0, 0, JUMP_DELAY, 0);
            for (int i = 0; i < num1 / 2; i++)
            {
                // genLineDataNewly(speed, 1, X_Start+2*i*delta_x1, Y_Start, Z_Start+2*i*delta_z1, X_Start+2*i*delta_x1, Y1, Z_Start+2*i*delta_z1);
                // genDelayData(X_Start+2*i*delta_x1, Y1, Z_Start+2*i*delta_z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
                // genLineDataNewly(JUMP_SPEED, 0, X_Start+2*i*delta_x1, Y1, Z_Start+2*i*delta_z1, X_Start+(2*i+1)*delta_x1, Y1, Z_Start+(2*i+1)*delta_z1);
                // genDelayData(X_Start+(2*i+1)*delta_x1, Y1, Z_Start+(2*i+1)*delta_z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
                // genLineDataNewly(speed, 1, X_Start+(2*i+1)*delta_x1, Y1, Z_Start+(2*i+1)*delta_z1, X_Start+(2*i+1)*delta_x1, Y_Start, Z_Start+(2*i+1)*delta_z1);
                // genDelayData(X_Start+(2*i+1)*delta_x1, Y_Start, Z_Start+(2*i+1)*delta_z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
                // genLineDataNewly(JUMP_SPEED, 0, X_Start+(2*i+1)*delta_x1, Y_Start, Z_Start+(2*i+1)*delta_z1, X_Start+(2*i+2)*delta_x1, Y_Start, Z_Start+(2*i+2)*delta_z1);
                // genDelayData(X_Start+(2*i+2)*delta_x1, Y_Start, Z_Start+(2*i+2)*delta_z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);

                genLineDataNewly(speed, 1, X_Start+2*i*delta_x1, Y_Start, Z_Start+2*i*delta_z1, X_Start+2*i*delta_x1, Y1, Z_Start+2*i*delta_z1);
                genDelayData(X_Start+2*i*delta_x1, Y1, Z_Start+2*i*delta_z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
                genLineDataNewly(JUMP_SPEED, 0, X_Start+2*i*delta_x1, Y1, Z_Start+2*i*delta_z1, X_Start+(2*i+1)*delta_x1, Y_Start, Z_Start+(2*i+1)*delta_z1);
                genDelayData(X_Start+(2*i+1)*delta_x1, Y_Start, Z_Start+(2*i+1)*delta_z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);

                genLineDataNewly(speed, 1, X_Start+(2*i+1)*delta_x1, Y_Start, Z_Start+(2*i+1)*delta_z1, X_Start+(2*i+1)*delta_x1, Y1, Z_Start+(2*i+1)*delta_z1);
                genDelayData(X_Start+(2*i+1)*delta_x1, Y1, Z_Start+(2*i+1)*delta_z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
                genLineDataNewly(JUMP_SPEED, 0, X_Start+(2*i+1)*delta_x1, Y1, Z_Start+(2*i+1)*delta_z1, X_Start+(2*i+2)*delta_x1, Y_Start, Z_Start+(2*i+2)*delta_z1);
                genDelayData(X_Start+(2*i+2)*delta_x1, Y_Start, Z_Start+(2*i+2)*delta_z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            }
            
                double t1 = num1 / 2; //最后一条不出光
                genLineDataNewly(speed, 1, X_Start+2*t1*delta_x1, Y_Start, Z_Start+2*t1*delta_z1, X_Start+2*t1*delta_x1, Y1, Z_Start+2*t1*delta_z1);
                genDelayData(X_Start+2*t1*delta_x1, Y1, Z_Start+2*t1*delta_z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);

        }

        if (beta == 90)  //竖扫
        {
            double delta_y1 = interval;
            double num2 = Y_length / delta_y1;
            genLineDataNewly(JUMP_SPEED, 0, 0, 0, 0, X_Start, Y_Start, Z_Start);
            //JumpDelay
            genDelayData(X_Start, Y_Start, Z_Start, 0, 0, JUMP_DELAY, 0);
            for (int i = 0; i < num2 / 2; i++)
            {
                // genLineDataNewly(speed, 1, X_Start, Y_Start+2*i*delta_y1, Z_Start, X1, Y_Start+2*i*delta_y1, Z1);
                // genDelayData(X1, Y_Start+2*i*delta_y1, Z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
                // genLineDataNewly(JUMP_SPEED, 0, X1, Y_Start+2*i*delta_y1, Z1, X1, Y_Start+(2*i+1)*delta_y1, Z1);
                // genDelayData(X1, Y_Start+(2*i+1)*delta_y1, Z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
                // genLineDataNewly(speed, 1, X1, Y_Start+(2*i+1)*delta_y1, Z1, X_Start, Y_Start+(2*i+1)*delta_y1, Z_Start);
                // genDelayData(X_Start, Y_Start+(2*i+1)*delta_y1, Z_Start, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
                // genLineDataNewly(JUMP_SPEED, 0, X_Start, Y_Start+(2*i+1)*delta_y1, Z_Start, X_Start, Y_Start+(2*i+2)*delta_y1, Z_Start);
                // genDelayData(X_Start, Y_Start+(2*i+2)*delta_y1, Z_Start, 0, 0, POLYGON_DELAY, POLYGON_DELAY);

                genLineDataNewly(speed, 1, X_Start, Y_Start+2*i*delta_y1, Z_Start, X1, Y_Start+2*i*delta_y1, Z1);
                genDelayData(X1, Y_Start+2*i*delta_y1, Z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
                genLineDataNewly(JUMP_SPEED, 0, X1, Y_Start+2*i*delta_y1, Z1, X_Start, Y_Start+(2*i+1)*delta_y1, Z_Start);
                genDelayData(X_Start, Y_Start+(2*i+1)*delta_y1, Z_Start, 0, 0, POLYGON_DELAY, POLYGON_DELAY);

                genLineDataNewly(speed, 1, X_Start, Y_Start+(2*i+1)*delta_y1, Z_Start, X1, Y_Start+(2*i+1)*delta_y1, Z1);
                genDelayData(X1, Y_Start+(2*i+1)*delta_y1, Z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
                genLineDataNewly(JUMP_SPEED, 0, X1, Y_Start+(2*i+1)*delta_y1, Z1, X_Start, Y_Start+(2*i+2)*delta_y1, Z_Start);
                genDelayData(X_Start, Y_Start+(2*i+2)*delta_y1, Z_Start, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
            }
                double t2 = num2 / 2;  //最后一条不出光
                genLineDataNewly(speed, 1, X_Start, Y_Start+2*t2*delta_y1, Z_Start, X1, Y_Start+2*t2*delta_y1, Z1);
                genDelayData(X1, Y_Start+2*t2*delta_y1, Z1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
        }
        //计算步长

        if (beta > 0 && beta < 90) 
        {
            beta = beta * Math.PI / 180;

            double delta_y = interval / Math.Sin(beta);
            double delta_x = interval / Math.Cos(beta) * Math.Cos(alpha);
            double delta_z = interval / Math.Cos(beta) * Math.Sin(alpha);

            double k = -1 * (delta_x / delta_y); //轨迹投影的斜率
            double b1 = X1 - k * Y1;
            double b2 = X_Start - k * Y_Start;
            double num = 2 * (b1 - b2) / delta_x;

            //public static void genLineDataNewly(double speed, int Laser_Switch, double X1, double Y1, double Z1, double X2, double Y2, double Z2)
            //不出光Jump至起点
            genLineDataNewly(JUMP_SPEED, 0, 0, 0, 0, X_Start, Y_Start, Z_Start);
            //JumpDelay
            genDelayData(X_Start, Y_Start, Z_Start, 0, 0, JUMP_DELAY, 0);

            double x_e = X_Start;
            double y_e = Y_Start;
            double z_e = Z_Start;

            double x_s;
            double y_s;
            double z_s;

            for (int i = 1; i <= num; i++)
            {
                x_s = x_e;             //JUMP1
                y_s = y_e;
                z_s = z_e;

                if (x_s + delta_x < X_LimL)     //在左边界上
                {
                    x_e = x_s + delta_x;
                    y_e = Y_Start;
                    z_e = z_s + delta_z;
                    genLineDataNewly(JUMP_SPEED, 0, x_s, y_s, z_s, x_e, y_e, z_e);
                    genDelayData(x_e, y_e, z_e, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
                }

                else if (x_s + delta_x > X_LimL) //超出左边界
                {
                    if (x_s + delta_x <= X_LimL + k * (Y_Start - Y_LimR))
                    {
                        y_e = y_s - (x_s + delta_x - X_LimL) / k;
                        x_e = X_LimL;
                        z_e = Z_LimL;
                        genLineDataNewly(JUMP_SPEED, 0, x_s, y_s, z_s, x_e, y_e, z_e);
                        genDelayData(x_e, y_e, z_e, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
                    }
                    else if (x_s + delta_x > X_LimL + k * (Y_Start - Y_LimR))
                    {
                        break;
                    }
                }

                x_s = x_e;             //LINE1
                y_s = y_e;
                z_s = z_e;

                if (y_s + (X_Start - x_s) / k < Y_LimR)     //在下边界上
                {
                    x_e = X_Start;
                    y_e = y_s + (X_Start - x_s) / k;
                    z_e = Z_Start;
                    genLineDataNewly(speed, 1, x_s, y_s, z_s, x_e, y_e, z_e);
                    genDelayData(x_e, y_e, z_e, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
                }

                else if (y_s + (X_Start - x_s) / k > Y_LimR) //超出下边界
                {
                    if (y_s + (X_Start - x_s) / k <= Y_LimR + (X_Start - X_LimL) / k)
                    {
                        y_e = Y_LimR;
                        x_e = k * (Y_LimR - y_s) + x_s;
                        z_e = Z_Start + (x_e - X_Start) * Math.Tan(alpha);
                        genLineDataNewly(speed, 1, x_s, y_s, z_s, x_e, y_e, z_e);
                        genDelayData(x_e, y_e, z_e, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
                    }
                    else if (y_s + (X_Start - x_s) / k > Y_LimR + (X_Start - X_LimL) / k)
                    {
                        break;
                    }
                }

                x_s = x_e;             //JUMP2
                y_s = y_e;
                z_s = z_e;

                if (y_s + delta_y < Y_LimR)     //在下边界上
                {
                    x_e = X_Start;
                    y_e = y_s + delta_y;
                    z_e = Z_Start;
                    genLineDataNewly(JUMP_SPEED, 0, x_s, y_s, z_s, x_e, y_e, z_e);
                    genDelayData(x_e, y_e, z_e, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
                }

                else if (y_s + delta_y > Y_LimR) //超出下（右）边界
                {
                    if (y_s + delta_y <= Y_LimR + (X_Start - X_LimL) / k)
                    {
                        x_e = k * (Y_LimR - y_s) + x_s + delta_x;
                        y_e = Y_LimR;
                        z_e = Z_Start + (x_e - X_Start) * Math.Tan(alpha);
                        genLineDataNewly(JUMP_SPEED, 0, x_s, y_s, z_s, x_e, y_e, z_e);
                        genDelayData(x_e, y_e, z_e, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
                    }
                    else if (y_s + delta_y > Y_LimR + (X_Start - X_LimL) / k)
                    {
                        break;
                    }
                }

                x_s = x_e;             //LINE2
                y_s = y_e;
                z_s = z_e;

                if (x_s + k * (Y_Start - y_s) < X_LimL)     //在左边界上
                {
                    x_e = x_s + k * (Y_Start - y_s);
                    y_e = Y_Start;
                    z_e = Z_Start + (x_e - X_Start) * Math.Tan(alpha);
                    genLineDataNewly(speed, 1, x_s, y_s, z_s, x_e, y_e, z_e);
                    genDelayData(x_e, y_e, z_e, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
                }

                else if (x_s + k * (Y_Start - y_s) > X_LimL) //超出左（上）边界
                {
                    if (x_s + k * (Y_Start - y_s) <= X_LimL + k * (Y_Start - Y_LimR))
                    {
                        y_e = y_s + (X_LimL - x_s) / k;
                        x_e = X_LimL;
                        z_e = Z_LimL;
                        genLineDataNewly(speed, 1, x_s, y_s, z_s, x_e, y_e, z_e);
                        genDelayData(x_e, y_e, z_e, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
                    }
                    else if (x_s + k * (Y_Start - y_s) > X_LimL + k * (Y_Start - Y_LimR))
                    {
                        break;
                    }
                }
            }

        }
        
    }

        public static void genFilled_LineDataly_convex_hust(double speed, double X0, double Y0, double Z0, double R, double theta_1, double theta_2, double d_y)
    //(x0y0z0为中心坐标，theta1,theta2为弧形角度，d_y为宽度)
    {
        d_y = d_y*1/0.95;
        double radtheta_1 = theta_1 * Math.PI / 180;
        double radtheta_2 = theta_2 * Math.PI / 180;

        double X_min = X0 + R * Math.Sin((radtheta_2+radtheta_1)*0.5);
        double Z_min = Z0 + R * Math.Cos((radtheta_2+radtheta_1)*0.5);


        //左上角坐标
            double X_Start = X0 + R * Math.Sin(radtheta_1);
            double Y_Start = Y0-0.5*d_y;
            double Z_Start = Z0 + R * Math.Cos(radtheta_1);
        //左下角坐标
            double X_End = X0 + R * Math.Sin(radtheta_2);
            double Y_End = Y0-0.5*d_y;
            double Z_End = Z0 + R * Math.Cos(radtheta_2);
        

        //"H"
            genLineDataNewly(JUMP_SPEED, 0, 0, 0, 0, X_Start, Y_Start, Z_Start);
            genDelayData(X_Start, Y_Start, Z_Start, 0, 0, JUMP_DELAY, 0);   //JumpDelay
            gen3DCircle_LineDataly_convex(speed, 1, X0, Y_Start, Z0, R, theta_1, theta_2);
            genDelayData(X_End, Y_End, Z_End, 0, 0, POLYGON_DELAY, POLYGON_DELAY);

            genLineDataNewly(JUMP_SPEED, 0, X_End, Y_End, Z_End, X_min, Y_Start, Z_min);
            genDelayData(X_min, Y_Start, Z_min, 0, 0, JUMP_DELAY, 0);   //JumpDelay
            genLineDataNewly(speed, 1, X_min, Y_Start, Z_min, X_min, Y_Start+0.2*d_y, Z_min);
            genDelayData(X_min, Y_Start+0.2*d_y, Z_min, 0, 0, JUMP_DELAY, 0);   //JumpDelay

            genLineDataNewly(JUMP_SPEED, 0, X_min, Y_Start+0.2*d_y, Z_min, X_Start, Y_Start+0.2*d_y, Z_Start);
            genDelayData(X_Start, Y_Start+0.2*d_y, Z_Start, 0, 0, JUMP_DELAY, 0);   //JumpDelay
            gen3DCircle_LineDataly_convex(speed, 1, X0, Y_Start+0.2*d_y, Z0, R, theta_1, theta_2);
            genDelayData(X_End, Y_End+0.2*d_y, Z_End, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
        //"U"
            genLineDataNewly(JUMP_SPEED, 0, X_End, Y_End+0.2*d_y, Z_End, X_Start, Y_Start+0.25*d_y, Z_Start);
            genDelayData(X_Start, Y_Start+0.25*d_y, Z_Start, 0, 0, JUMP_DELAY, 0);   //JumpDelay
            gen3DCircle_LineDataly_convex(speed, 1, X0, Y_Start+0.25*d_y, Z0, R, theta_1, theta_2);
            genDelayData(X_End, Y_End+0.25*d_y, Z_End, 0, 0, POLYGON_DELAY, POLYGON_DELAY);

            genLineDataNewly(speed, 1, X_End, Y_End+0.25*d_y, Z_End, X_End, Y_End+0.45*d_y, Z_End);
            genDelayData(X_End, Y_End+0.45*d_y, Z_End, 0, 0, POLYGON_DELAY, POLYGON_DELAY);   //JumpDelay
            gen3DCircle_LineDataly_convex(speed, 1, X0, Y_End+0.45*d_y, Z0, R, theta_2, theta_1);
            genDelayData(X_Start, Y_Start+0.45*d_y, Z_Start, 0, 0, POLYGON_DELAY, POLYGON_DELAY);   //JumpDelay
        //"S"
            genLineDataNewly(JUMP_SPEED, 0, X_Start, Y_Start+0.45*d_y, Z_Start, X_Start, Y_Start+0.7*d_y, Z_Start);
            genDelayData(X_Start, Y_Start+0.7*d_y, Z_Start, 0, 0, JUMP_DELAY, 0);   //JumpDelay
            genLineDataNewly(speed, 1, X_Start, Y_Start+0.7*d_y, Z_Start, X_Start, Y_Start+0.5*d_y, Z_Start);
            genDelayData(X_Start, Y_Start+0.5*d_y, Z_Start, 0, 0, POLYGON_DELAY, POLYGON_DELAY);   //JumpDelay
            
            gen3DCircle_LineDataly_convex(speed, 1, X0, Y_End+0.5*d_y, Z0, R, theta_1, (theta_2+theta_1)*0.5);
            genDelayData(X_min, Y_Start+0.5*d_y, Z_min, 0, 0, POLYGON_DELAY, POLYGON_DELAY);   //JumpDelay
            
            genLineDataNewly(speed, 1, X_min, Y_Start+0.5*d_y, Z_min, X_min, Y_Start+0.7*d_y, Z_min);
            genDelayData(X_min, Y_Start+0.7*d_y, Z_min, 0, 0, POLYGON_DELAY, POLYGON_DELAY);   //JumpDelay

            gen3DCircle_LineDataly_convex(speed, 1, X0, Y_End+0.7*d_y, Z0, R, (theta_2+theta_1)*0.5, theta_2);
            genDelayData(X_End, Y_End+0.7*d_y, Z_End, 0, 0, POLYGON_DELAY, POLYGON_DELAY);   //JumpDelay
            
            genLineDataNewly(speed, 1, X_End, Y_End+0.7*d_y, Z_End, X_End, Y_End+0.5*d_y, Z_End);
            genDelayData(X_End, Y_End+0.5*d_y, Z_End, 0, 0, POLYGON_DELAY, POLYGON_DELAY);   //JumpDelay
        //"T"
            genLineDataNewly(JUMP_SPEED, 0, X_End, Y_End+0.5*d_y, Z_End, X_Start, Y_Start+0.75*d_y, Z_Start);
            genDelayData(X_Start, Y_Start+0.75*d_y, Z_Start, 0, 0, JUMP_DELAY, 0);   //JumpDelay
            genLineDataNewly(speed, 1, X_Start, Y_Start+0.75*d_y, Z_Start, X_Start, Y_Start+0.95*d_y, Z_Start);
            genDelayData(X_Start, Y_Start+0.95*d_y, Z_Start, 0, 0, POLYGON_DELAY, POLYGON_DELAY);   //JumpDelay

            genLineDataNewly(JUMP_SPEED, 0, X_Start, Y_Start+0.95*d_y, Z_Start, X_Start, Y_Start+0.85*d_y, Z_Start);
            genDelayData(X_Start, Y_Start+0.85*d_y, Z_Start, 0, 0, JUMP_DELAY, 0);   //JumpDelay
            gen3DCircle_LineDataly_convex(speed, 1, X0, Y_End+0.85*d_y, Z0, R, theta_1, theta_2);
            genDelayData(X_End, Y_End+0.85*d_y, Z_End, 0, 0, POLYGON_DELAY, POLYGON_DELAY);   //JumpDelay

    }
    

    public static void test1()
    {


        //genLineDataNew(double speed, int Laser_Switch, double X1,
        //double Y1, double Z1, double A1, double B1, double X2, double Y2,
        //double Z2, double A2, double B2)
        DataBuffer.addProcessBegin  ();

        //----------------------刘翌大论文实验-坐标系校正------------------------
        // genLineDataNewly(JUMP_SPEED, 0, 0, 0, 0, 0, 2, 0);
        // genDelayData(0, 2, 0, 0, 0, JUMP_DELAY, 0);   //JumpDelay
        // genLineDataNewly(5, 1, 0, 2, 0, 0, 1, 0);
        // genDelayData(0, 1, 0, 0, 0, JUMP_DELAY, 0);   //JumpDelay
        // genLineDataNewly(5, 1, 0, 1, 0, 1, 1, 0);
        // genDelayData(1, 1, 0, 0, 0, JUMP_DELAY, 0);   //JumpDelay
        // genLineDataNewly(5, 1, 1, 1, 0, 1, 2, 0);
        // genDelayData(1, 2, 0, 0, 0, JUMP_DELAY, 0);   //JumpDelay
        // genLineDataNewly(5, 1, 1, 2, 0, 0, 2, 0);
        // genDelayData(0, 2, 0, 0, 0, JUMP_DELAY, 0);   //JumpDelay
        // genLineDataNewly(JUMP_SPEED, 0, 0, 0, 0, 1, 1, 0);
        // genDelayData(1, 1, 0, 0, 0, JUMP_DELAY, 0);   //JumpDelay
        // genLineDataNewly(5, 1, 1, 1, 0, 1, 2, 0);
        // genDelayData(1, 2, 0, 0, 0, JUMP_DELAY, 0);   //JumpDelay
        // genLineDataNewly(JUMP_SPEED, 1, 2, 0, 0, 1, 1, 0);
        // genDelayData(1, 1, 0, 0, 0, JUMP_DELAY, 0);   //JumpDelay
        // genLineDataNewly(5, 1, 1, 1, 0, 2, 1, 0);
        // genDelayData(2, 1, 0, 0, 0, JUMP_DELAY, 0);   //JumpDelay
        //genFilledRectangleData3D_beta(0, 0, 0, 22, 22,0,  2000, 5, 0);
        //直线_三轴振镜
        // -----------------------------------20251208CHQ（刘翌保存）--------------------------
         double X_Start = 0;//-0.1
         double Y_Start = 0;
         double Z_Start = 0;//-8+1.5;

         double Z_Test = Z_Start;
         double Y_Length = 20;
         double X_Length = 20;
         double X_interval = 0.2;//线间隔
              
        // // // //          //十字 
        for (int j = 1; j <=10;j++)//扫描次数
        {
         genLineDataNewly(1000, 0, X_Start, Y_Start, Z_Start, 0, -0.5*Y_Length, Z_Test);//挪至起点;Y方向
         genDelayData(0, -0.5*Y_Length, Z_Test, 0, 0, POLYGON_DELAY, POLYGON_DELAY);//起点位置
         genLineDataNewly(20, 1, 0, -0.5*Y_Length, Z_Test, 0, 0.5*Y_Length, Z_Test);//(double speed, int Laser_Switch, double X1, double Y1, double Z1, double X2, double Y2, double Z2)
         genDelayData(0, 0.5 * Y_Length, Z_Test, 0, 0, POLYGON_DELAY, POLYGON_DELAY);//终点位置

         genLineDataNewly(1000, 0, X_Start, Y_Start, Z_Start, -0.5*X_Length, 0, Z_Test);//X方向
         genDelayData(-0.5*X_Length, 0, Z_Test, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
         genLineDataNewly(20, 1, -0.5*X_Length, 0, Z_Test, 0.5*X_Length, 0, Z_Test);//(double speed, int Laser_Switch, double X1, double Y1, double Z1, double X2, double Y2, double Z2)
         genDelayData(0.5*X_Length, 0, Z_Test, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
        }
             //单条线；Y方向：
        // -----------------------------------20251208CHQ（刘翌保存）--------------------------
    //   for (int n = 1; n <= 10; n++)//n为扫描次数
    //     {
    //         genLineDataNewly(2000, 0, X_Start, Y_Start, Z_Start, X_Start, -0.5 * Y_Length, Z_Test);
    //         genDelayData(X_Start, -0.5 * Y_Length, Z_Test, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
    //         genLineDataNewly(20, 1, X_Start, -0.5 * Y_Length, Z_Test, X_Start, 0.5 * Y_Length, Z_Test);//(double speed, int Laser_Switch, double X1, double Y1, double Z1, double X2, double Y2, double Z2)
    //         genDelayData(X_Start, 0.5 * Y_Length, Z_Test, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
    //     }
        //      //矩形
        //      //genFilledRectangleData3D_Y2(0, 0, 0, 0.75, 0.75, 0, 2000, 0.5);//上平面
        //      //genFilledRectangleData3D_X2(0, 0, 0, 0.75, 0.75, 0, 2000, 0.5);
        //      //genFilledRectangleData3D_Y2(0, 0, -5, 0.75, 0.75, -5, 2000, 0.5);//下平面
        //      //genFilledRectangleData3D_X2(0, 0, -5, 0.75, 0.75, -5, 2000, 0.5);
       
       
        //扫描次数递增的线列：
        
        // for (int j = 1; j <= 20; j++)
        // {
        // for (int n = 1; n <= j; n++)//n为扫描次数
        // {
        // genLineDataNewly(2000, 0, X_Start, Y_Start, Z_Start, X_Start, -0.5 * Y_Length, Z_Test);//(double speed, int Laser_Switch, double X1, double Y1, double Z1, double X2, double Y2, double Z2)
        // genDelayData(X_Start, -0.5 * Y_Length, Z_Test, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
        // genLineDataNewly(2000, 1, X_Start, -0.5 * Y_Length, Z_Test, X_Start, 0.5 * Y_Length, Z_Test);
        // genDelayData(X_Start, 0.5 * Y_Length, Z_Test, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
        // }
        // X_Start = X_Start + X_interval;
        // }


        //genFilledRectangleData3D_Y2(0, 0, -5, 2.5, 2.5, -5, 2000, 0.5);
        //genFilledRectangleData3D_X2(0, 0, -5, 2.5, 2.5, -5, 2000, 0.5);

        // for (int j = 0; j <= 5; j++)
        // {
        //     genCircleNewly(0, 0, 3, 3, 0, 50);
        // }
        //圆_三轴振镜 public static void genCircleNewly(double X0, double Y0, double X1, double Y1,double Z1, double speed)
        //genCircleNewly(0,0,3,4,-10,2000);
        //填充3D矩形
        //genFilledRectangleData3D(double X0, double Y0, double Z0, double X1, double Y1, double Z1, double speed, double interval )//(X0,Y0,Z0)为中心，(X1,Y1,Z1)为右上角点
        //genFilledRectangleData3D_Y2(0, 0, 0, 5, 5, 5, 50, 0.5);
        //genFilledRectangleData3D_X(0, 0, 5, 5, 7.07, 10, 2000, 0.1 );

        //genFilledRectangleData3D_Yord(0, 0, 3, 2.828, 4, 5.828, 8000, 0.1);


        //genFilledRectangleData3D_Y_and_Y2(3, -5, -9, 23, 15, -9, 2000, 0.5);//实验一参数，无透镜，重复频率120k，功率53%，手动Z轴示数15.5,loop=10，实际焦平面Z=-10
        //genFilledRectangleData3D_Y_and_Y2(0, 0, 0, 1.5, 2, 0, 2000, 0.5);//实验一参数，无透镜，重复频率120k，功率53%，手动Z轴示数15.5,loop=10，实际焦平面Z=-10  ***0814




        //genFilledRectangleData3D_Y_and_Y2(2, -3, -9, 22, 17, -9, 2000, 0.5);//实验一参数，无透镜，重复频率120k，功率53%，手动Z轴示数15.5,loop=10，实际焦平面Z=-10
        //genFilledRectangleData3D_Y_and_Y2(3, -5, 0, 23, 15, 0, 2000, 0.5);//实验二参数，无透镜，重复频率120k，功率53%，手动Z轴示数5.5,loop=10，实际焦平面Z=0
        //genFilledRectangleData3D_Y_and_Y2(3, -5, -9, 23, 15, -9, 2000, 0.5);//实验三（无）参数，无透镜，重复频率120k，功率63%，手动Z轴示数15.5,loop=10，实际焦平面Z=-10

        //20230815*****
        //genFilledRectangleData3D_Y_and_Y2(1, -4.5, -8.5, 21, 15.5, -9, 2000, 0.5);//实验一（无）参数，无透镜，重复频率85k，脉宽100ns，功率47%，手动Z轴示数5.5,loop=10，实际焦平面Z=0



        //genFilledRectangleData3D_X_and_X2(1, -4.5, -8.8, 21, 15.5, -8.8, 2000, 0.5);

        //genFilledRectangleData3D_X_and_X2(1, -4.5, -8.8, 21, 15.5, -8.8, 2000, 0.5);//with，z=-10,100ns，85khz，52%，Z轴位移示数12.365 loop10_real
        //genFilledRectangleData3D_X_and_X2(1, -4.5, -8.8, 21, 15.5, -8.8, 2000, 0.5);//without，z=-10,100ns，85khz，50%，Z轴位移示数12.365 loop10
        //genFilledRectangleData3D_X_and_X2(1, -6, 0, 21, 14, 0, 2000, 0.5);//without/with，z=0,100ns，85khz，52%，Z轴位移示数4.545_real
        //genFilledRectangleData3D_X_and_X2(0, 0, 0, 20, 20, 0, 2000, 0.5);
        //genFilledRectangleData3D_Y_and_Y2(0, 0, 0, 20, 20, 0, 2000, 0.5);//实验三（无）参数，无透镜，重复频率85k，脉宽100ns，功率47%，手动Z轴示数15.5,loop=10，实际焦平面Z=-10
        //填充同心圆
        //genCircle_filledly(double X0, double Y0, double X1, double Y1, double Z1, double speed, double interval)
        //genCircle_filledly(0, 0, 3, 4, 0, 50, 0.1);



        //genFilledRectangleData3D_X_and_X2(0, 0, -10, 5, 5, -10, 2500, 0.5);//without/with，z=0,100ns，85khz，52%，Z轴位移示数4.545
        //genCircleFilledDataly(double X0, double Y0, double X1, double Y1, double z_start, double speed, double r_min, double r_interval)
        //genCircleFilledDataly(0, 0, 3, 4, 0, 2000, 0.1, 0.1);
        //genFilledRectangleData3D_till(0, 0, -2.652, 2.652, 7.5, 0, 8000, 0.25);
        //genFilledRectangleData3D_Y_and_Y2(0, 0, -1.768, 1.768, 2.5, 0, 200, 0.1);//0912
        //double num_Loop = 10;
        //for (int i = 0; i <= num_Loop; i++)
        //{
        //genRectangleData3D(0, 0, -3.304, 5.304, 7.5, 2, 8000, 0.1);//回型填充
        //}

        //genFilledRectangleData3D_till(0, 0, 3, 2.828, 4, 5.828, 8000, 0.1);

        //实验：横、纵、斜
        //genFilledRectangleData3D_Y_and_Y2(0, 0, 2, 7.072, 10, 9.072, 8000, 0.25);
        //实验：横、斜
        //genFilledRectangleData3D_Y_and_Y2(6, 0, 0, 9.535533905, 5, 3.535533905, 500, 0.1768);

        //gen3DCircle_LineDataly(double speed, int Laser_Switch, double X0, double Y0, double Z0, double R, double theta_1, double theta_2)
        //（X0, Y0, Z0）为起点.theta为角度制，theta定义为theta对应的P点，与起点连线，这条线和Z轴所成的锐角，连线向+X为正，向-X为负。
        // double num_Loop = 5;
        // for (int i = 0; i <= num_Loop; i++)
        // {
        //gen3DCircle_LineDataly_convex(80000, 1, 0, 0, -35, 50, 25, -25);
        //     //genLineDataNewly(200, 1, -5, 0, 0, 5, 0, 0);
        // }

        //public static void gen3DCircle_LineDataly_convex_filled(double speed, double X0, double Y0, double Z0, double R, double theta_1, double theta_2, double width_Y, double interval_Y)

        // gen3DCircle_LineDataly_convex_filled(130000, 0, 0, -15, 25, 30, -30, 20, 1);//(double speed, double X0, double Y0, double Z0, double R, double theta_1, double theta_2, double width_Y, double interval_Y)

        //for (int i = 0; i <= 9; i++)
        //{
        //genFilledRectangleData3D_Y_and_Y2(6, 0, 0, 9.535533905, 5, 3.535533905, 3000, 0.1);   
        //}






       //正方形扫描
        //  double alpha = 45;//加工平面与与水平面的倾角
        //  alpha = alpha * Math.PI / 180;
        //  double length = 20;//边长
        //  double x0 = 0+0.028;
        //  double y0 = 0-3*length;
        //  double x1 = 0.5 * length * Math.Cos(alpha);
        //  double y1 = 0.5 * length;
        //  double z1 = 0.5 * length * Math.Sin(alpha);
        //  //genFilledRectangleData3D_beta(0, 0, 0, x1, y1, z1, 2000, 0.2, 0);//genFilledRectangleData3D_beta(double X0, double Y0, double Z0, double X1, double Y1, double Z1, double speed, double interval, double beta)；beta为在加工平面上的扫描方向

        //  for (int i = 0; i <= 6 ; i++)
        // {
        //    genFilledRectangleData3D_beta(0, 0 - 3*length + i*length, 0, x1, y1 - 3*length + i*length, z1, 200, 0.15, 0 + i*15);
        // }


        //genLineDataNewly(JUMP_SPEED, 0, 0, 0, 0, 1, -0.5, -1);
        //genDelayData(1, -0.5, -1, 0, 0, JUMP_DELAY, 0);
        //genLineDataNewly(20, 1, -0.5, -1, -0.5, -0.5, 1, 0.5);
        //genDelayData(-0.5, -0.5, 1, 0, 0, POLYGON_DELAY, POLYGON_DELAY);


        //genFilledRectangleData3D_beta(0, 0, 0, 1.414, 2, 1.414, 700, 0.02, 0); 
        //genFilledRectangleData3D_beta(0, 0, 0, 3.535533905, 5, 3.535533905, 500, 0.25, 90); 
        //genRectangleData3D(0, 0, 0, 1, 1, 0, 3000, 0.1);//回型填充


        // genFilledRectangleData3D_Y_and_Y2(0, 0, 0, 3.535533905, 5, 3.535533905, 2000, 0.25);
        // genFilledRectangleData3D_Y_and_Y2(6, 0, 0, 9.535533905, 5, 3.535533905, 500, 0.1768);
        //}

        // for (int i = 0; i <= 9; i += 1) ////60度实验
        // {
        // genFilledRectangleData3D_beta(0, -8, 0, 1, -6, 1.7320508075688, 5000, 0.2, 0);     
        // }
        // for (int i = 0; i <= 9; i += 1)
        // {
        // genFilledRectangleData3D_beta(0, -4, 0, 1, -2, 1.7320508075688, 5000, 0.2, 22.5);     
        // }
        // for (int i = 0; i <= 9; i += 1)
        // {   
        // genFilledRectangleData3D_beta(0, 0, 0, 1, 2, 1.7320508075688, 5000, 0.2, 45); 
        // }
        // for (int i = 0; i <= 9; i += 1)
        // {
        // genFilledRectangleData3D_beta(0, 4, 0, 1, 6, 1.7320508075688, 5000, 0.2, 67.5); 
        // }
        // for (int i = 0; i <= 9; i += 1)
        // {
        // genFilledRectangleData3D_beta(0, 8, 0, 1, 10, 1.7320508075688, 5000, 0.2, 90); 
        // }

        // for (int i = 0; i <= 9; i += 1) ///45度实验
        // {
        // genFilledRectangleData3D_beta(0, -8, 0, 1.41421356, -6, 1.41421356, 5000, 0.2, 0);     
        // }
        // for (int i = 0; i <= 9; i += 1)
        // {
        // genFilledRectangleData3D_beta(0, -4, 0, 1.41421356, -2, 1.41421356, 5000, 0.2, 22.5);     
        // }
        // for (int i = 0; i <= 9; i += 1)
        // {   
        // genFilledRectangleData3D_beta(0, 0, 0, 1.41421356, 2, 1.41421356, 5000, 0.2, 45); 
        // }
        // for (int i = 0; i <= 9; i += 1)
        // {
        // genFilledRectangleData3D_beta(0, 4, 0, 1.41421356, 6, 1.41421356, 5000, 0.2, 67.5); 
        // }
        // for (int i = 0; i <= 9; i += 1)
        // {
        // // genFilledRectangleData3D_beta(0, 8, 0, 1.41421356, 10, 1.41421356, 5000, 0.2, 90); 
        // }

        // for (int i = 0; i <= 1; i += 1) ///30度实验
        // {
        // // genFilledRectangleData3D_beta(0, 0, 0, 0.5, 0.4, 0, 500, 0.1, 90);  
        // // genLineDataNewly(1000, 1, 0, 0, 0, 0, 0.8, 0);   
        // }
        // for (int i = 0; i <= 9; i += 1)
        // {
        // genFilledRectangleData3D_beta(0, -4, 0, 1.7320508075688, -2, 1, 500, 0.2, 22.5);     
        // }
        // for (int i = 0; i <= 9; i += 1)
        // {   
        // genFilledRectangleData3D_beta(0, 0, 0, 1.7320508075688, 2, 1, 500, 0.2, 45); 
        // }
        // for (int i = 0; i <= 9; i += 1)
        // {
        // genFilledRectangleData3D_beta(0, 4, 0, 1.7320508075688, 6, 1, 500, 0.2, 67.5); 
        // }
        // for (int i = 0; i <= 9; i += 1)
        // {
        // genFilledRectangleData3D_beta(0, 8, 0, 1.7320508075688, 10, 1, 500, 0.2, 90); 
        // }

        //  for (int i = 0; i <= 34; i += 1) ///
        // {
        // genFilledRectangleData3D_beta(0, 0, 0, 1.7320508075688, 2, 1, 5000, 0.2, 90); 
        // }

        //厚铜模实验
        //for (int i = 0; i <= 90; i += 2)
        //{   
        //genFilledRectangleData3D_beta(0, 0, 0, 5, 5, 0, 1000, 0.1, i); 
        //}

        //genLineDataNewly(13000, 0, 0, 0, 0, 1.414, 2, 1.414);
        //JumpDelay
        //genDelayData(0, 0, 0, 0, 0, JUMP_DELAY, 0);

        //genRectangleData3D(double X0, double Y0, double Z0, double X1, double Y1, double Z1, double speed, double Y_interval)//回型填充
        //genRectangleData3D(0, 0, 0, 5, 5, 0, 1000, 0.1);//回型填充
        //for (int i = 0; i <= 38; i += 1)
        //{    
        // genFilledRectangleData3D_beta(0, 0, 5, 3, 4, 5, 2000, 0.1, 0); 
        //genFilledRectangleData3D_beta(0, -8, 0, 1.414, -6, 1.414, 5000, 0.1, 90); 
        //genLineDataNewly(50, 1, 0, -5, 0, 0, 5, 0);
        //}
        //genFilledRectangleData3D_beta(3.75, 0, 0, 5, 5, 0, 1000, 0.1, 0); 
        //genFilledRectangleData3D_beta(0, -3.75, 0, 2.5, -2.5, 0, 1000, 0.1, 0); 
        //genFilledRectangleData3D_beta(0, 3.75, 0, 2.5, 5, 0, 1000, 0.1, 0); 
        //genFilledRectangleData3D_beta(-3.75, 0, 0, -2.5, 5, 0, 1000, 0.1, 0); 

        //public static void gen3DCircle_LineDataly_convex(double speed, int Laser_Switch, double X0, double Y0, double Z0, double R, double theta_1, double theta_2)
        // //（X0, Y0, Z0）为起点.theta为角度制，theta定义为theta对应的P点，与起点连线，这条线和Z轴所成的锐角，连线向+X为正，向-X为负。
        // //     //genLineDataNewly(200, 1, -5, 0, 0, 5, 0, 0);
        //genFilled_LineDataly_convex_hust(double speed, double X0, double Y0, double Z0, double R, double theta_1, double theta_2, double d_y)

        // double d = 0.1;
        // double y_start = -20;
        // gen3DCircle_LineDataly_convex(7000, 1, 0, y_start+2*i*d-10, -10, 25, 30, -30);
        // gen3DCircle_LineDataly_convex(7000, 1, 0, y_start+(2*i+1)*d-10, -10, 25, -30, 30);

        // for (int j = 0; j <= 0; j++)
        // {
        //     genFilledRectangleData3D_beta(0, 0, 0, 0.5, 2, 0, 2000, 0.2, 0); 
        // }
        // gen3DCircle_LineDataly_convex(500, 1, 0, 0, 20, 25, 135, 225);
        //     // {
        //     // genLineDataNewly(2000, 1, 0, -5, 5, 0, -10, 5);
        //     // genLineDataNewly(2000, 1, 0, -12.5, 5, 0, -15, 5);
        //     // }
        // -----------------------------------------
        // double R = 25;
        // double radtheta_1 = 30 * Math.PI / 180;
        // double x_start = R * Math.Sin(radtheta_1);
        // double x_start = 0;
        // double y_start = 0;
        // double z_start = 25 - 1.11;// + R *Math.Cos(radtheta_1);
        // double speed = 50;
        // double interval_Y = 0.2;

        // for (int i = 0; i <= 10; i++)
        // {
        //     gen3DCircle_LineDataly_convex(speed, 1, x_start, y_start + i * interval_Y, z_start, 25, 150, 210);
        //gen3DCircle_LineDataly_convex(double speed, int Laser_Switch, double X0, double Y0, double Z0, double R, double theta_1, double theta_2)
        // }

        //      //Y方向：
        // for (int i = 0; i <=6; i++)
        // {        
        //  double R = 25;
        //  double theta = (135 + i*15)*Math.PI/180;
        //  double Y_length = 10;
        //  double X_Start = R * Math.Sin(theta);
        //  double Y_Start = -Y_length / 2;
        //  double Y_End = Y_length / 2;
        //  double Z0 = 0;
        //  double Z_Start =Z0 + R *(1+ Math.Cos(theta));
        //  double Z_End = Z_Start;
        //     genLineDataNewly(20, 0, X_Start, Y_Start, Z_Start, X_Start, Y_End, Z_End);
        //     genDelayData(X_Start, Y_Start, Z_Start, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
        //     genLineDataNewly(20, 1, X_Start, Y_Start, Z_Start, X_Start, Y_End, Z_End);//(double speed, int Laser_Switch, double X1, double Y1, double Z1, double X2, double Y2, double Z2)
        //     genDelayData(X_Start, Y_End, Z_End, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
        // }

        // genLineDataNewly(JUMP_SPEED, 0, 0, 0, 0, x_start, y_start, z_start);
        // //JumpDelay
        // genDelayData(x_start, y_start, z_start, 0, 0, JUMP_DELAY, 0);

        // for (int i = 0; i <= 750; i++)
        // {
        // double d = 0.01;
        // double speed = 3000;

        // gen3DCircle_LineDataly_convex(speed, 1, 0, y_start+2*i*d, 20, 25, 135, 225);
        // gen3DCircle_LineDataly_convex(speed, 1, 0, y_start+(2*i+1)*d, 20, 25, 225, 135);
        // }
        // -----------------------------------------


        //     // gen3DCircle_LineDataly_convex(speed, 1, 0, y_start+2*i*d, -20, 25, -10, -20);
        //     // gen3DCircle_LineDataly_convex(speed, 1, 0, y_start+(2*i+1)*d, -20, 25, -20, -10);

        //     // gen3DCircle_LineDataly_convex(speed, 1, 0, y_start+2*i*d, -20, 25, -25, -35);
        //     // gen3DCircle_LineDataly_convex(speed, 1, 0, y_start+(2*i+1)*d, -20, 25, -35, -25);

        //     // gen3DCircle_LineDataly_convex(speed, 1, 0, y_start+2*i*d, -20, 25, -40, -50);
        //     // gen3DCircle_LineDataly_convex(speed, 1, 0, y_start+(2*i+1)*d, -20, 25, -50, -40);

        //     // gen3DCircle_LineDataly_convex(speed, 1, 0, y_start+2*i*d, -20, 25, -10, -20);
        //     // gen3DCircle_LineDataly_convex(speed, 0, 0, y_start+2*i*d, -20, 25, -20, -25);

        //     // gen3DCircle_LineDataly_convex(speed, 1, 0, y_start+2*i*d, -20, 25, -25, -35);
        //     // gen3DCircle_LineDataly_convex(speed, 0, 0, y_start+2*i*d, -20, 25, -35, -40);

        //     // gen3DCircle_LineDataly_convex(speed, 1, 0, y_start+2*i*d, -20, 25, -40, -50);
        //     // gen3DCircle_LineDataly_convex(speed, 0, 0, y_start+2*i*d, -20, 25, -50, -53);


        //     // gen3DCircle_LineDataly_convex(speed, 1, 0, y_start+(2*i+1)*d, -20, 25, -50, -40);
        //     // gen3DCircle_LineDataly_convex(speed, 0, 0, y_start+(2*i+1)*d, -20, 25, -40, -35);

        //gen3DCircle_LineDataly_convex(speed, 1, 0, y_start+(2*i+1)*d, -20, 25, -35, -25);
        //     // gen3DCircle_LineDataly_convex(speed, 0, 0, y_start+(2*i+1)*d, -20, 25, -25, -20);

        //     // gen3DCircle_LineDataly_convex(speed, 1, 0, y_start+(2*i+1)*d, -20, 25, -20, -10);
        //     // gen3DCircle_LineDataly_convex(speed, 0, 0, y_start+(2*i+1)*d, -20, 25, -10, -5);

        //     // gen3DCircle_LineDataly_convex(speed, 1, 0, y_start+(2*i+1)*d, -20, 25, -5, 5);
        //     // gen3DCircle_LineDataly_convex(speed, 0, 0, y_start+(2*i+1)*d, -20, 25, 5, 8);



        //     double d = 0.02;
        //     double y_start = -10;
        //     double speed_1 = 2000;
        //     double speed_2 = 2000;
        //     int num = 30;

        //  for (int j = 0; j <= 0; j++)
        //  {
        //     for (int k = 0; k <= 500; k++)
        //     {
        //         for (int i = 0; i <= num/2-1; i++)
        //         {
        //         gen3DCircle_LineDataly_convex(speed_1+(speed_2-speed_1)/(num/2-1)*i, 1, 0, y_start+2*k*d, -20, 25, 45-3*i, 45-3*(i+1));
        //         }   
        //         for (int i = num/2; i <= num-1; i++)
        //         {
        //         gen3DCircle_LineDataly_convex(speed_2+(speed_1-speed_2)/(num/2-1)*(i-num/2), 1, 0, y_start+2*k*d, -20, 25, 45-3*i, 45-3*(i+1));
        //         }

        //         for (int i = 0; i <= num/2-1; i++)
        //         {
        //         gen3DCircle_LineDataly_convex(speed_1+(speed_2-speed_1)/(num/2-1)*i, 1, 0, y_start+(2*k+1)*d, -20, 25, -45+3*i, -45+3*(i+1));
        //         }
        //         for (int i = num/2; i <= num-1; i++)
        //         {
        //         gen3DCircle_LineDataly_convex(speed_2+(speed_1-speed_2)/(num/2-1)*(i-num/2), 1, 0, y_start+(2*k+1)*d, -20, 25, -45+3*i, -45+3*(i+1));
        //         }
        //      }
        // }



        //CHQ曲面线列
        // ↓
        // double R = 25;
        // double speed = 1000;
        // double X0 = -0.1;//0.08-0.2//若X正方向缺线，需调小//圆心空间坐标
        // double Y0 = 0;
        // double Z0 = 25-2;//使零点处的焦平面落在需要加工的Z范围中间
        // double theta_1 = 135;//起始角度
        // double theta_2 = 225;
        // double width_Y = 30;
        // double interval_Y = 0.2;
         
        // for (int i = 1; i <= 10; i++)//扫描次数i
        // {
        //     gen3DCircle_LineDataly_convex_filled(speed, X0, Y0, Z0, R, theta_1, theta_2, width_Y, interval_Y);
        // }

        //  for (int i = 0; i <= 10; i++)
        // {
        //      gen3DCircle_LineDataly_convex(speed, 1, X0, Y0 + i * interval_Y, Z0, R, theta_1, theta_2);//gen3DCircle_LineDataly_convex(double speed, int Laser_Switch, double X0, double Y0, double Z0, double R, double theta_1, double theta_2)
        // }
        // ↑

        //CHQ斜面焦点线列
    //      double X_interval = 0.04;//线间隔
    //      double X_Start = 0-10*X_interval+0.098;//在45度斜面上修正量+0.098；在60或30度斜面上不需要修正;该实验将焦平面对准1.1mm厚的导电玻璃表面;
    //      double Y_Start = 0;
    //      double Z_Start = 0-0.098;//修正规则类比X_Start
    //      double Z_Test = Z_Start;
    //      double Y_Length = 2.5;
         
    //   for (int n = 0; n <= 20; n++)//n为扫描次数
    //     {
    //         genLineDataNewly(2000, 0, X_Start, Y_Start, Z_Start, X_Start, -0.5 * Y_Length, Z_Test);
    //         genDelayData(X_Start, -0.5 * Y_Length, Z_Test, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
    //         genLineDataNewly(2000, 1, X_Start, -0.5 * Y_Length, Z_Test, X_Start, 0.5 * Y_Length, Z_Test);//(double speed, int Laser_Switch, double X1, double Y1, double Z1, double X2, double Y2, double Z2)
    //         genDelayData(X_Start, 0.5 * Y_Length, Z_Test, 0, 0, POLYGON_DELAY, POLYGON_DELAY);
    //         X_Start = X_Start + X_interval;
    //     }

         //CHQ正方形扫描实验↓
        //  double H = 1;
        //  double delta = 0.098;//Math.Cos(45*Math.PI)*(1.1-H)/Math.Cos (15*Math.PI/180);//材料厚度相对于导电玻璃变化时，焦平面坐标的变化量
        //  double alpha = 45;//加工平面与与水平面的倾角
        //  alpha = alpha * Math.PI / 180;
        //  double length = 3;//边长
        //  double x0 = 0+delta;//(x0,y0,z0)是正方形中心坐标，(x1,y1,z1)为右上角坐标
        //  double y0 = 0;//-3.5*length;
        //  double z0 = 0-delta;
        //  double x1 = x0 + 0.5 * length * Math.Cos(alpha);
        //  double y1 = y0 + 0.5 * length;
        //  double z1 = z0 + 0.5 * length * Math.Sin(alpha);    
//  for(int n = 1; n <= 10 ; n++)
//  {
//         //  for (int i = 0; i <= 6 ; i++)
//         // {
//         // genFilledRectangleData3D_beta(x0, y0 + i*length, z0, x1, y1 + i*length, z1, 1000, 0.1, 0 + i*15);
//         // }
//         genFilledRectangleData3D_beta(x0, y0, z0, x1, y1, z1, 1000, 0.1, 0);
// }
//         //genFilledRectangleData3D_beta(0, 0, 0, x1, y1, z1, 500, 0.15, 90);
//         //上行代码解释：genFilledRectangleData3D_beta(double X0, double Y0, double Z0, double X1, double Y1, double Z1, double speed, double interval, double beta)；beta为在加工平面上的扫描方向
//         //  ↑


        DataBuffer.addProcessEnd();
    }

    

public static void test2()
    {
        DataBuffer.addProcessBegin();


        //斜方孔加工
        //genProcessSquareHole();


        //实验二、变间距m值对底面的影响
        //double m = 0.00000000;
        //double repair_times = 7;
        //double a = 0.0315;
        //double n = 0.00048;
        //double z_interval = 0.045;
        //double Scantimes = 21;
        //double z_end = -(Scantimes - 1) * z_interval;
        //gen2DSpialLineDataNewChangeSpacingtaperRepair(300, 1, a, m, n, -6, 0.38, 0, 0, 0, z_end, z_interval, repair_times);


        //盲孔加工第二次，20kHz，5%，1.26W 2022.6.26
        // double a = 0.031;
        // double m = -0.0000005;
        // double n = 0.0011;
        // double processing_times = 2;
        // double z_start = -0.86;
        // double z_end = -0.87;
        // double z_interval = Math.Abs(z_start - z_end) / (processing_times - 1);

        // gen2DSpialLineDataNewChangeSpacingtaperRepair(300, 1, a, m, n, -6, 0.38, 0, 0, z_start, z_end, z_interval, 2);

        //功率控制测试
        // DataBuffer.setPowerData(10);
        // genCircleData(100,1,0,0,5,0,1,360,-3,true,1,0.5,0,0,0,0,0);
        //DataBuffer.setPowerData(90);
        //genCircleData(100,1,0,0,5,0,1,360,-3,true,1,0.5,0,0,0,0,0);
        //DataBuffer.setPowerData(20);
        //genCircleData(100,1,0,0,5,0,1,360,-3,true,1,0.5,0,0,0,0,0);
        //DataBuffer.setPowerData(80);
        //genCircleData(100,1,0,0,5,0,1,360,-3,true,1,0.5,0,0,0,0,0);
        //DataBuffer.setPowerData(30);
        //genCircleData(100,1,0,0,5,0,1,360,-3,true,1,0.5,0,0,0,0,0);
        //DataBuffer.setPowerData(70);
        //genCircleData(100,1,0,0,5,0,1,360,-3,true,1,0.5,0,0,0,0,0);
        //DataBuffer.setPowerData(40);
        //genCircleData(100,1,0,0,5,0,1,360,-3,true,1,0.5,0,0,0,0,0);
        //DataBuffer.setPowerData(60);
        //genCircleData(100,1,0,0,5,0,1,360,-3,true,1,0.5,0,0,0,0,0);
        
        
        //实验一、填充间距
        //double a = 0.031;
        //double spacing = 0.02;
        //double b = spacing / (2*3.14); 
        //double processing_times = 1;
        //double z_interval = 0.025;
        //gen2DSpialLineDataNewChangetaper(300, processing_times, a, b, -6, 0.38, 0, 0, 0, -0.5, z_interval);

        //实验二、变间距m值对底面的影响
        //double a = 0.031;
        //double m = -0.00000021;
        //double n = 0.0011;
        //double z_interval = 0.045;
        //gen2DSpialLineDataNewChangeSpacingtaperRepair(300, 1, a, m, n, -6, 0.38, 0, 0, 0, -0.5, z_interval, 0);

        //实验三、变间距加修边
        //double a = 0.031;
        //double m = -0.00000041;
        //double n = 0.0011;
        //double z_interval = 0.045;
        //gen2DSpialLineDataNewChangeSpacingtaperRepair(300, 1, a, m, n, -5, 0.38, 0, 0, 0, -0.5, z_interval, 6);

    }

}