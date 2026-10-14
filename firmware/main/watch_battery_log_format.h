#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#define BATLOG_MAGIC 0x314c5442U
#define BATLOG_VERSION 1
#define BATLOG_SLOTS 8
#define BATLOG_RECORDS 64
#define BATLOG_INTERVAL_S 180U
enum { B_BATTERY=1,B_USB=2,B_CHARGING=4,B_STATUS_OK=8,B_VOLT_OK=16,B_PERCENT_OK=32,
       B_TIME_OK=64,B_WIFI=128,B_IP=256,B_WIFI_OFF_POLICY=512,B_POWER=1024,
       B_DFS=2048,B_TICK=4096,B_SCENE=8192,B_SLEEP=16384 };
typedef struct {
 uint32_t seq,epoch_s,uptime_s;
 uint16_t battery_mv,flags;
 uint8_t percent,brightness,screen,reserved,cpu_clients,awake_clients,network_clients,reserved2;
} batlog_record_t;
typedef struct {
 uint32_t magic;uint16_t version,record_size;uint32_t start_seq,count,crc;
 batlog_record_t records[BATLOG_RECORDS];
} batlog_chunk_t;
_Static_assert(sizeof(batlog_record_t)==24,"Battery record layout");
_Static_assert(offsetof(batlog_chunk_t,records)==20,"Battery header layout");
static inline uint32_t batlog_crc(const void *buf,size_t n) {
 const uint8_t *p=buf;uint32_t crc=~0U;
 for(size_t i=0;i<n;i++){crc^=p[i];for(unsigned b=0;b<8;b++)crc=(crc>>1)^(0xedb88320U&(0U-(crc&1U)));}
 return ~crc;
}
static inline size_t batlog_size(const batlog_chunk_t *c){return 20+c->count*sizeof(batlog_record_t);}
static inline bool batlog_valid(const batlog_chunk_t *c,size_t size,unsigned slot) {
 return size>=20&&c->magic==BATLOG_MAGIC&&c->version==BATLOG_VERSION&&c->record_size==24&&
 c->count>0&&c->count<=BATLOG_RECORDS&&c->start_seq%BATLOG_RECORDS==0&&
 (c->start_seq/BATLOG_RECORDS)%BATLOG_SLOTS==slot&&size==batlog_size(c)&&
 c->crc==batlog_crc(c->records,c->count*sizeof(batlog_record_t));
}
