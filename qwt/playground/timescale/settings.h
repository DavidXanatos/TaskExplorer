#ifndef _SETTINGS_H_
#define _SETTINGS_H_ 1

#include <qdatetime.h>

class Settings
{
public:
    Settings():
        maxMajorSteps( 10 ),
        maxMinorSteps( 5 ),
        maxWeeks( -1 )
    {
    };

    QDateTime startDateTime;
    QDateTime endDateTime;

    int maxMajorSteps;
    int maxMinorSteps;

    int maxWeeks;
};

#endif
