//#ifdef MOTOR_FILLUP
//#define MOTOR_FILLUP
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval

bool fillupDoneFlag = false;


const int startHours[] = {11, 23/*, 16*/};
const int startMinutes[] = {30, 0/*, 56*/};

// out of range values are allowed for mktime
// hoping this will work
const int endHours[] = {12, 23/*, 16*/};
const int endMinutes[] = {30, 45/*, 57*/};

const int n_intervals = sizeof(startHours)/sizeof(startHours[0]);


struct tm getTimeFromHourMinute(time_t now, int hour, int minute) {
  struct tm theTime;
  theTime = *gmtime(&now);
  theTime.tm_hour = hour;
  theTime.tm_min = minute;
  theTime.tm_sec = 0;
  return theTime;
}

bool isInInterval(time_t now, int startHour, int startMinute, int endHour, int endMinute) {
  struct tm startTime, endTime;
  startTime = getTimeFromHourMinute(now, startHour, startMinute);
  endTime = getTimeFromHourMinute(now, endHour, endMinute);
  return difftime(now, mktime(&startTime)) > 0 && difftime(now, mktime(&endTime)) < 0;
}

bool isTankFull(double distance) {
  return  distance < 1.05 * TANK_FULL_DISTANCE_CM; // 5% threshold
}


////////// FIX THIS:
//// should turn off after interval is done even if water is not full
bool tankFillUp(int motor, bool tankFull, time_t now, int startHour, int startMinute, int endHour, int endMinute) {
  // return true if in interval, otherwise false
  if (! isInInterval(now, startHour, startMinute, endHour, endMinute)) {
    return false;
  }
  
  if (motor && tankFull) {
    writeMotor(LOW);
    fillupDoneFlag = true;
  } else if (!motor && !fillupDoneFlag) {
    writeMotor(HIGH);
  }
  
  return true;
}

void fixWhenNoInterval(bool tankFull, bool motor) {
  // if motor is on but not manually, then switch it off
  if (motor && !isManuallySwitchedOnFlag) {
    writeMotor(LOW);
  }
  // just make flag false and turn off motor if full
  if (tankFull)
    writeMotor(LOW);
  fillupDoneFlag = false;
}

void doFillUp(Status s1) {
  bool inSomeInterval = false;
  bool tankFull = isTankFull(s1.distance);
  snprintf(msg, 100, "motor: %d, fillupDoneFlag: %d, tankFull: %d", s1.motor, fillupDoneFlag, tankFull);
  Serial.println(msg);
  for (int i = 0; i < n_intervals; i++) {
    inSomeInterval = inSomeInterval || tankFillUp(s1.motor, tankFull, s1.timestamp, startHours[i], startMinutes[i], endHours[i], endMinutes[i]);
  }
  if ( !inSomeInterval) {
    fixWhenNoInterval(tankFull, s1.motor);
  }
}



//#endif
