#include <vector>

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QtMath>

#include <QSqlQueryModel>

#include "loadtrace.h"

#include "sql/connection.h"

static t_Traces Traces;
static t_Extras Extras;

t_Trace *getTrace(int index)
{
    if (index < Traces.size())
    {
        return &Traces[index];
    }
    else
    {
        return nullptr;
    }
}

t_Extra *getExtra(int index)
{
    if (index < Extras.size())
    {
        return &Extras[index];
    }
    else
    {
        return nullptr;
    }
}

void processExclusions(QDir dir)
{
    if (!createConnection(dir))
        return;

    for(int end = Traces.size(), i = 0; i < end; i ++)
    {
        QSqlQuery query;
        query.prepare("select id, exclusion from trace where (filename=:filename and datetime=:datetime)");
        query.bindValue(":filename", Traces[i].fileName);
        query.bindValue(":datetime", Traces[i].dt.toSecsSinceEpoch());
        query.exec();
        if (query.first())
        {
            Traces[i].exclusion = query.value(1).toUInt();
        }
        else
        {
            Traces[i].exclusion = 0;
        }
    }
}

static QVector<qreal> makeBlackmanWindow(void)
{
    QVector<qreal> res;
    int M = 22;

    for (int i = 1; i < M; i ++)   // 1 .. (M-1).  Outside that, is zero
    {
        qreal iM = 2*M_PI*static_cast<qreal>(i)/static_cast<qreal>(M);
        res.push_back(   0.42
                        -0.50*qCos(iM)
                        +0.08*qCos(2*iM) );
    }
    return res;
}

static qreal BlackmanWindowSum(void)
{
    // The sum of the window shape above
    return 9.2400;
}

// Change a 3-dimensional trace to a 1-dimensional one
static QVector<qreal> ValsToRms(std::vector<std::array<float,3>> in)
{
    QVector<qreal> out;
    qreal x_avg, y_avg, z_avg;
    x_avg = y_avg = z_avg = 0.0;
    for(int end = in.size(), i = 0; i < end; i ++)
    {
        x_avg += in.at(i).at(0);
        y_avg += in.at(i).at(1);
        z_avg += in.at(i).at(2);
    }
    x_avg /= static_cast<qreal>(in.size());
    y_avg /= static_cast<qreal>(in.size());
    z_avg /= static_cast<qreal>(in.size());
    for(int end = in.size(), i = 0; i < end; i ++)
    {
        out.push_back( qSqrt(qPow(in.at(i).at(0)-x_avg, 2) + qPow(in.at(i).at(1)-y_avg, 2) + qPow(in.at(i).at(2)-z_avg, 2) )  );
    }
    return out;
}

static qreal convolve(QVector<qreal> longer, QVector<qreal> shorter, int offset)
{
    qreal tot = 0.;
    for(int end = shorter.size(), i = 0; i < end; i ++)
    {
        tot += shorter.at(i)*longer.at(i + offset);
    }
    return tot;
}

void addWindowedMax(void)
{
    QVector<qreal> theWindow = makeBlackmanWindow();

    int latestBase = -1;
    bool isExcluded = false;
    qreal latestTot = 0.;
    QDateTime lastTime = QDateTime(); // invalid

    for(int end = Traces.size(), i = 0; i < end; i ++)
    {
        if (lastTime.isValid() && Traces.at(i).dt < lastTime.addSecs(6))
        {
            // This is not sufficiently long after the previous trace -- probably part of the same event.

        }
        else
        {
            // A new event. Push the old one.
            if (latestBase >= 0 && !isExcluded)
            {
                t_Trace &newT = Traces[latestBase];
                newT.wMax = 16384.0f*static_cast<float>(1.414213562 * latestTot / BlackmanWindowSum());  // Scale by 16384 to change it back into measurement units.
                                                                                            // Scale by SQRT(2) to account for RMS.
            }

            latestBase = i;
            latestTot = 0.;
            isExcluded = false;
        }

        lastTime = Traces.at(i).dt;

        if (Traces.at(i).exclusion > 0)
        {
            isExcluded = true;
        }

        if (isExcluded)
        {
            // do nothing
        }
        else
        {
            // Calculate the maximum
            int j = 0;
            QVector<qreal> valsrms = ValsToRms(Traces.at(i).vals);
            qreal max_y = -99999.;
            for (int end = valsrms.size() - theWindow.size() + 1; j < end; j ++)
            {
                qreal y = convolve(valsrms, theWindow, j);
                if (y > max_y)
                {
                    max_y = y;
                }
            }

            if (max_y > latestTot)
            {
                latestTot = max_y;
            }
        }
    }

    // Push the final value.
    if (latestBase >= 0 && !isExcluded)
    {
        t_Trace &newT = Traces[latestBase];
        newT.wMax = 16384.0f*static_cast<float>(1.414213562 * latestTot / BlackmanWindowSum());  // Scale by 16384 to change it back into measurement units.
                                                                                                 // Scale by SQRT(2) to account for RMS.
    }

}

t_VDVs postProcessVdv(void)
{
    t_VDVs vs;
    // Day is 7AM - 11 PM
    // Night is 11 PM - 7AM.
    const int morning_hour = 7;
    const int evening_hour = 23;

    for(int endi = Traces.size(), i = 0; i < endi; i ++)
    {
        QDateTime dt_start = Traces.at(i).dt;
        int j, endj;
        for (endj = vs.size(), j = 0; j < endj; j ++)
        {
            if (dt_start >= vs.at(j).start && dt_start <= vs.at(j).end)
            {
                // This matches
                vs[j].total_VDV += qPow(Traces.at(i).total4thPowerDeviation, 4.0);
                break;
            }
        }
        if (j >= endj)
        {
            // None matched
            QDateTime dt_end = dt_start;
            int h = dt_start.time().hour();
            if (h >= 0 && h < morning_hour)
            {
                dt_end.setTime(QTime(morning_hour, 0, 0));
                dt_start = dt_end.addDays(-1);
                dt_start.setTime(QTime(evening_hour, 0, 0));
            }
            else if (h < evening_hour)
            {
                dt_start.setTime(QTime(morning_hour, 0, 0));
                dt_end.setTime(QTime(evening_hour, 0, 0));
            }
            else
            {
                dt_start.setTime(QTime(evening_hour, 0, 0));
                dt_end = dt_start.addDays(1);
                dt_end.setTime(QTime(morning_hour, 0, 0));
            }
            t_VDV v;
            v.start = dt_start;
            v.end = dt_end;
            v.total_VDV = qPow(Traces.at(i).total4thPowerDeviation, 4.0);
            vs.push_back(v);
        }

    }
    for (int endj = vs.size(), j = 0; j < endj; j ++)
    {
        vs[j].total_VDV = qPow(vs[j].total_VDV, 0.25);
    }

    return vs;
}

static void AddNewTrace(t_Trace &trace)
{
    unsigned int i;
    qreal x_sum=0., y_sum=0., z_sum=0.;
    size_t max_i = trace.vals.size();
    const qreal freq = 125.0;   // TODO: make this depend on the input

    trace.frequency = static_cast<float>(freq);
    for(i = 0; i < max_i; i ++)
    {
        trace.vals[i][0] = trace.vals[i][0] / 16384.0f;
        trace.vals[i][1] = trace.vals[i][1] / 16384.0f;
        trace.vals[i][2] = trace.vals[i][2] / 16384.0f;
    }
    for(i = 0; i < max_i; i ++)
    {
        x_sum += static_cast<qreal>(trace.vals[i][0]); y_sum += static_cast<qreal>(trace.vals[i][1]); z_sum += static_cast<qreal>(trace.vals[i][2]);
    }

    qreal x_avg = x_sum/(static_cast<qreal>(max_i));
    qreal y_avg = y_sum/(static_cast<qreal>(max_i));
    qreal z_avg = z_sum/(static_cast<qreal>(max_i));

    qreal max_sq_dev = 0.;
    qreal sum_sq_dev = 0.;
    qreal sum_4thpow = 0.;
    qreal max_sq_dev_per_axis[3];
    max_sq_dev_per_axis[0] = 0.; max_sq_dev_per_axis[1] = 0.; max_sq_dev_per_axis[2] = 0.;

    for(i = 0; i < max_i; i ++)
    {
        qreal sq_dev = 0.;
        qreal axis_sq_dev;

        // X axis
        axis_sq_dev = qPow(static_cast<qreal>(trace.vals[i][0]) - x_avg, 2);
        sq_dev += axis_sq_dev;
        if (axis_sq_dev > max_sq_dev_per_axis[0])
            max_sq_dev_per_axis[0] = axis_sq_dev;


        // Y axis
        axis_sq_dev = qPow(static_cast<qreal>(trace.vals[i][1]) - y_avg, 2);
        sq_dev += axis_sq_dev;
        if (axis_sq_dev > max_sq_dev_per_axis[1])
            max_sq_dev_per_axis[1] = axis_sq_dev;


        // Z axis
        axis_sq_dev = qPow(static_cast<qreal>(trace.vals[i][2]) - z_avg, 2);
        sq_dev += axis_sq_dev;
        if (axis_sq_dev > max_sq_dev_per_axis[2])
            max_sq_dev_per_axis[2] = axis_sq_dev;


        if (sq_dev > max_sq_dev)
        {
            max_sq_dev = sq_dev;
        }
        sum_sq_dev += sq_dev;
        sum_4thpow += qPow(sq_dev, 2);
    }

    trace.maximumDeviation = 16384.0f*static_cast<float>(qSqrt(max_sq_dev));
    trace.rmsDeviation = 16384.0f*static_cast<float>(qSqrt(sum_sq_dev/static_cast<qreal>(max_i)));
    trace.total4thPowerDeviation = static_cast<float>(qPow(sum_4thpow/freq, 0.25));

    if (max_sq_dev_per_axis[0] < max_sq_dev_per_axis[1])
    {
        if (max_sq_dev_per_axis[1] < max_sq_dev_per_axis[2])
        {
            trace.maxAxis = 2;
        }
        else
        {
            trace.maxAxis = 1;
        }
    }
    else
    {
        if (max_sq_dev_per_axis[0] < max_sq_dev_per_axis[2])
        {
            trace.maxAxis = 2;
        }
        else
        {
            trace.maxAxis = 0;
        }
    }
    trace.wMax = 0.;

    Traces.push_back(trace);
}

t_Traces * loadtrace(QDir fDir, QList<QFileInfo> fFiles)
{
    QDateTime dt = QDateTime::currentDateTime();

    Traces.clear();

    if (fFiles.isEmpty())
    {
        fFiles = fDir.entryInfoList(QDir::Files);
    }

    foreach (QFileInfo fInfo, fFiles)
    {
        if (fInfo.suffix().toLower() == "csv")
        {
            int traces_in_file = 0;
            QFile file(fInfo.filePath());
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                std::vector<std::array<float, 3>> xyz;

                float v_bat, temp_1, temp_2;

                v_bat = -1.0; temp_1 = -1.0; temp_2 = -1.0;
                xyz.clear();

                QTextStream in(&file);
                while (!in.atEnd()) {
                    QString line = in.readLine();

                    QStringList s3 = line.split(QChar(','));

                    if (s3.length() == 3 && s3[2]!= "")
                    {
                        // Seems to be a data line. Add it on.
                        std::array<float, 3> m;
                        bool ok = true;
                        if (!!ok)
                        {
                            m[0] = s3[0].toFloat(&ok);
                        }
                        if (!!ok)
                        {
                            m[1] = s3[1].toFloat(&ok);
                        }
                        if (!!ok)
                        {
                            m[2] = s3[2].toFloat(&ok);
                        }
                        if (!!ok)
                        {
                            xyz.push_back(m);
                        }
                    }
                    else
                    {
                        // Not a data line.
                        if (xyz.size() > 0)
                        {
                            t_Trace Trace;
                            Trace.isOn = false;
                            Trace.isHeartbeat = false;
                            Trace.dt = dt;
                            Trace.vals = xyz;
                            Trace.indexInFile = traces_in_file;
                            Trace.fileName = fInfo.fileName();
                            xyz.clear();
                            AddNewTrace(Trace);
                            traces_in_file ++;
                            v_bat = -1.0; temp_1 = -1.0; temp_2 = -1.0;
                        }

                        int pos;
                        pos = line.indexOf("Vbat=");
                        if (pos >= 0)
                        {
                            QStringRef subString(&line, pos+5, line.length()-pos-5);
                            v_bat = subString.split(" ")[0].toFloat();
                        }
                        pos = line.indexOf("Tint=");
                        if (pos >= 0)
                        {
                            QStringRef subString(&line, pos+5, line.length()-pos-5);
                            temp_1 = subString.split(" ")[0].toFloat();
                        }
                        pos = line.indexOf("Tacc=");
                        if (pos >= 0)
                        {
                            QStringRef subString(&line, pos+5, line.length()-pos-5);
                            temp_2 = subString.split(" ")[0].toFloat();
                        }

                        if (line[2] == '/')
                        {
                            // Looks like a datetime line
                            dt = QDateTime::fromString(line, "dd/MM/yyyy,HH:mm:ss,");
                            dt.setTimeSpec(Qt::UTC);
                        }
                        else if (line.startsWith("HEARTBEAT"))
                        {
                           t_Extra extra;
                           extra.dt = dt;
                           extra.type = t_ExtraType::Heartbeat;
                           extra.v_bat = v_bat;
                           extra.temp_1 = temp_1;
                           extra.temp_2 = temp_2;
                           extra.fileName = fInfo.fileName();
                           v_bat = -1.0; temp_1 = -1.0; temp_2 = -1.0;
                           Extras.push_back(extra);

                        }
                        else if (line.startsWith("ON"))
                        {
                            t_Extra extra;
                            extra.dt = dt;
                            extra.type = t_ExtraType::On;
                            extra.v_bat = v_bat;
                            extra.temp_1 = temp_1;
                            extra.temp_2 = temp_2;
                            extra.fileName = fInfo.fileName();
                            v_bat = -1.0; temp_1 = -1.0; temp_2 = -1.0;
                            Extras.push_back(extra);
                        }
                        else
                        {

                        }
                    }
                }

                if (xyz.size() > 0)
                {
                    t_Trace Trace;
                    Trace.isOn = false;
                    Trace.isHeartbeat = false;
                    Trace.vals = xyz;
                    Trace.dt = dt;
                    Trace.indexInFile = traces_in_file;
                    Trace.fileName = fInfo.fileName();
                    xyz.clear();
                    AddNewTrace(Trace);
                    traces_in_file ++;
                    v_bat = -1.0; temp_1 = -1.0; temp_2 = -1.0;
                }

                file.close();
            }
        }
    }
    return &Traces;

}
