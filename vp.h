// SVP header
#include "payload.h"
#include <cstdint>
#include <inttypes.h>

class VP {
private:
  /////////////////////////////////////////////////////////////////////
  // Put private class variables here.
  /////////////////////////////////////////////////////////////////////

  /////////////////////////////////////////////////////////////////////
  // Structure 1: Value Prediction Queue
  // Entry contains: PC Tag/PC Index, Value
  /////////////////////////////////////////////////////////////////////

  typedef struct VPQEntry {
    int64_t Index;
    uint64_t Tag;
    uint64_t PC;
    int64_t Value;
    bool completed;
  } VPQEntry;

  typedef struct VPQTable {
    int Counter;
    uint64_t Head;
    uint64_t Tail;
    bool headPhase;
    bool tailPhase;
    VPQEntry *VPQEntries;
  } VPQTable;

  /////////////////////////////////////////////////////////////////////
  // Structure 2: Stride Value Predictor
  // Entry contains: Tag, Confidence, RetireValue, StrideValue, and Instance
  // Number.
  /////////////////////////////////////////////////////////////////////
  typedef struct SVPEntry {
    uint64_t Tag;
    uint64_t ConfNum;
    int64_t RetireVal;
    int64_t StrideVal;
    uint64_t InstanceVal;
  } SVPEntry;

  VPQTable VPQ;
  SVPEntry *SVP;
  int SVPEntries;

public:
  ////////////////////////////////////////
  // Stats
  ////////////////////////////////////////

  int IneligiblePred, EligiblePred, VPMiss, ConfCorrect, ConfIncorrect,
      UnconfCorrect, UnconfIncorrect;
  int IneligibleType, IneligibleDrop;

  ////////////////////////////////////////
  // Public functions.
  ////////////////////////////////////////

  /////////////////////////////////////////////////////////////////////
  // This is the constructor function.
  /////////////////////////////////////////////////////////////////////
  VP();

  /////////////////////////////////////////////////////////////////////
  // This is the destructor, used to clean up memory space and
  // other things when simulation is done.
  // I typically don't use a destructor; you have the option to keep
  // this function empty.
  /////////////////////////////////////////////////////////////////////
  //~VP();

  /////////////////////////////////////////////////////////////////////
  // Functions to implement SVP and VPQ
  /////////////////////////////////////////////////////////////////////

  bool stall_VP(unsigned int); // Check VPQ if there is enough space

  uint64_t init_NewPred(uint64_t); // Then initialize VPQ and SVP with new pred
  // entry and get VPQ Entry at tail.

  void predict(uint64_t &, bool &, uint64_t &, uint64_t, bool &);
  void retire_val(uint64_t, uint64_t); // Retire logic
  uint64_t get_tag(uint64_t);
  uint64_t get_index(uint64_t);

  void set_VPQVal(uint64_t, uint64_t);
  void resolveVPQ(uint64_t, bool);
  uint64_t getTail();
  bool getPhase();
  bool get_conf_counter(uint64_t,
                        uint64_t); // Once we initalize VPQ and SVP, you
                                   // need to get the confidence counter.
  void flush();
  void instCounterRollback(uint64_t, bool );
  void dump_stats(FILE *);
  int num_entries();
};
