#ifndef LOADTRACE_H
#define LOADTRACE_H

#include <QDateTime>
#include <QList>
#include <QDir>
#include <QTreeWidgetItem>
#include <QString>
#include <array>
#include <vector>

class t_Trace
{
public:
    bool isOn;
    bool isHeartbeat;
    QDateTime dt;
    float  maximumDeviation;
    float  rmsDeviation;
    float  total4thPowerDeviation;
    QString  fileName;
    int     indexInFile;
    int     indexInDir;
    uint    exclusion;
    float   frequency;
    float   wMax;   // windowed maximum

    int    maxAxis;  // axis of greatest deviation. 0 = X, 1 = Y, 2 = Z
    std::vector<std::array<float,3>> vals;
};

typedef enum
{
    Heartbeat = 1,   // hourly heartbeat record
    On = 2          // device reset

} t_ExtraType;

class t_Extra
{
public:
    t_ExtraType type;
    QDateTime   dt;
    QString    fileName;
    float v_bat;    // V
    float temp_1;   // deg C
    float temp_2;   // deg C
    float temp_3;   // deg C
};

class t_VDV
{
public:
    QDateTime start;
    QDateTime end;
    float     total_VDV;
};

typedef QVector<t_VDV> t_VDVs;

typedef QVector<t_Trace> t_Traces;

typedef QList<t_Extra> t_Extras;

extern t_Trace * getTrace(int index);
extern t_Extra * getExtra(int index);

extern t_Traces * loadtrace(QDir, QList<QFileInfo>);
extern void  processExclusions(QDir dir);
extern t_VDVs postProcessVdv(void);

extern void addWindowedMax(void);

#endif // LOADTRACE_H
