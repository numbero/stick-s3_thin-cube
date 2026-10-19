#pragma once
#include "tap_parameters.h"
#include <cJSON.h>
#include <math.h>
#include <string.h>

enum class TapAction { Configure, ResetCounters, RestoreRecommended };
inline bool tapReadRequest(cJSON* j,TapParameters& p,char* id,size_t capacity,TapAction& operation){
  const auto action=cJSON_GetObjectItemCaseSensitive(j,"action");
  const auto request=cJSON_GetObjectItemCaseSensitive(j,"requestId");
  if(!cJSON_IsString(action)||!cJSON_IsString(request))return false;
  const size_t n=strlen(request->valuestring);
  if(!n||n>=capacity)return false;
  for(size_t i=0;i<n;++i){const char c=request->valuestring[i];
    if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'))return false;
  }
  strcpy(id,request->valuestring);
  if(!strcmp(action->valuestring,"reset")){operation=TapAction::ResetCounters;return true;}
  if(!strcmp(action->valuestring,"restore")){operation=TapAction::RestoreRecommended;return true;}
  if(strcmp(action->valuestring,"configure"))return false;
  operation=TapAction::Configure;
  const auto cfg=cJSON_GetObjectItemCaseSensitive(j,"config");if(!cJSON_IsObject(cfg))return false;
  auto flag=[&](const char* key,bool& out){const auto v=cJSON_GetObjectItemCaseSensitive(cfg,key);
    if(!cJSON_IsBool(v))return false;out=cJSON_IsTrue(v);return true;};
  auto number=[&](const char* key,uint16_t& out){const auto v=cJSON_GetObjectItemCaseSensitive(cfg,key);
    if(!cJSON_IsNumber(v)||!isfinite(v->valuedouble)||v->valuedouble<0||v->valuedouble>10000||v->valuedouble!=uint16_t(v->valuedouble))return false;
    out=uint16_t(v->valuedouble);return true;};
  p.software=false;p.softGyroFilterMs=0;
  // Optional for older native-only clients; absent fields restore safe defaults.
  auto optionalFlag=[&](const char* key,bool& out){return !cJSON_GetObjectItemCaseSensitive(cfg,key)||flag(key,out);};
  auto optionalNumber=[&](const char* key,uint16_t& out){return !cJSON_GetObjectItemCaseSensitive(cfg,key)||number(key,out);};
  return flag("enabled",p.enabled)&&flag("single",p.single)&&flag("doubleTap",p.doubleTap)&&flag("triple",p.triple)&&
    flag("filtered",p.filtered)&&flag("wait",p.wait)&&flag("manual",p.manual)&&number("axis",p.axis)&&
    number("sensitivity",p.sensitivity)&&number("windowMs",p.windowMs)&&number("quietMs",p.quietMs)&&
    number("settleMs",p.settleMs)&&number("buttonGuardMs",p.buttonGuardMs)&&number("cooldownMs",p.cooldownMs)&&
    optionalFlag("software",p.software)&&optionalNumber("softThresholdMg",p.softThresholdMg)&&
    optionalNumber("softReboundMs",p.softReboundMs)&&optionalNumber("softWindowMs",p.softWindowMs)&&
    optionalNumber("softGyroDps",p.softGyroDps)&&optionalNumber("softMotionGuardMs",p.softMotionGuardMs)&&
    optionalNumber("softBaselineMs",p.softBaselineMs)&&optionalNumber("softRatioPercent",p.softRatioPercent)&&
    optionalNumber("softGyroFilterMs",p.softGyroFilterMs)&&p.valid();
}
