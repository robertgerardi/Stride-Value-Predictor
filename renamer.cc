#include "renamer.h"
#include <cassert>
#include <inttypes.h>

renamer::renamer(uint64_t n_log_regs, uint64_t n_phys_regs, uint64_t n_branches,
                 uint64_t n_active) {

  assert(n_phys_regs > n_log_regs);
  assert((1 <= n_branches) && (n_branches <= 64));
  assert(n_active > 0);

  RMT = new uint64_t[n_log_regs];
  AMT = new uint64_t[n_log_regs];
  mapTablesSize = n_log_regs;
  // set contents of AMT and RMT to match at first

  for (int i = 0; i < n_log_regs; i++) {
    RMT[i] = i;
    AMT[i] = i;
  }

  Freelist.head = 0;
  Freelist.tail = 0;
  Freelist.headPhase = 0;
  Freelist.tailPhase = 1;
  Freelist.freelistSize = n_phys_regs - n_log_regs;
  Freelist.listArray = new uint64_t[n_phys_regs - n_log_regs];

  for (int i = 0; i < (n_phys_regs - n_log_regs); i++) {
    Freelist.listArray[i] = n_log_regs + i;
  }

  ActiveList.head = 0;
  ActiveList.tail = 0;
  ActiveList.headPhase = 0;
  ActiveList.tailPhase = 0;
  ActiveList.active_list = new activeListEntry[n_active];
  ActiveList.activeListSize = n_active;

  for (int i = 0; i < n_active; i++) {
    ActiveList.active_list[i].destRegFlag = 0;
    ActiveList.active_list[i].logicalReg = 0;
    ActiveList.active_list[i].physicalReg = 0;
    ActiveList.active_list[i].completed = 0;
    ActiveList.active_list[i].exception = 0;
    ActiveList.active_list[i].loadViolation = 0;
    ActiveList.active_list[i].branchMidprediction = 0;
    ActiveList.active_list[i].valueMisprediction = 0;
    ActiveList.active_list[i].loadFlag = 0;
    ActiveList.active_list[i].storeFlag = 0;
    ActiveList.active_list[i].branchFlag = 0;
    ActiveList.active_list[i].amoFlag = 0;
    ActiveList.active_list[i].csrFlag = 0;
    ActiveList.active_list[i].PC = 0;
  }

  PRF = new uint64_t[n_phys_regs];
  PRFReadyBits = new bool[n_phys_regs];
  for (int i = 0; i < n_phys_regs; i++) {
    PRFReadyBits[i] = 1;
  }

  // implement gbm stuff
  GBM = 0;
  numCheckpoints = n_branches;
  BRCheckpoints = new checkpoints[n_branches];
}

bool renamer::stall_reg(uint64_t bundle_dst) {

  uint64_t numAvailableRegs;

  if (Freelist.head == Freelist.tail &&
      Freelist.headPhase == Freelist.tailPhase) {
    return true;
  }
  if (Freelist.head == Freelist.tail &&
      Freelist.headPhase != Freelist.tailPhase) {
    return false;
  }
  if (Freelist.headPhase != Freelist.tailPhase) {
    numAvailableRegs = Freelist.tail - Freelist.head + Freelist.freelistSize;
  }
  if (Freelist.headPhase == Freelist.tailPhase) {
    numAvailableRegs = Freelist.tail - Freelist.head;
  }

  if (numAvailableRegs < bundle_dst) {
    return true;
  } else {
    return false;
  }
}

bool renamer::stall_branch(uint64_t bundle_branch) {

  uint64_t numFreeCheckpoints = 0;
  uint64_t bitCheck = 1;
  for (int i = 0; i < numCheckpoints; i++) {
    // shifting bitcheck to check for regs
    if (GBM & bitCheck) {
      bitCheck <<= 1;
    } else {
      numFreeCheckpoints++;
      bitCheck <<= 1;
    }
  }

  // check if bundle can dispatch

  if (numFreeCheckpoints < bundle_branch) {
    return true;
  } else {
    return false;
  }
}

uint64_t renamer::get_branch_mask() { return GBM; }

uint64_t renamer::rename_rsrc(uint64_t log_reg) { return RMT[log_reg]; }

uint64_t renamer::rename_rdst(uint64_t log_reg) {
  // pop off freelist

  uint64_t poppedReg = Freelist.listArray[Freelist.head];

  Freelist.head++;

  if (Freelist.head == Freelist.freelistSize) {
    Freelist.head = 0;
    Freelist.headPhase = !Freelist.headPhase;
  }

  RMT[log_reg] = poppedReg;

  return poppedReg;
}

uint64_t renamer::checkpoint() {

  uint64_t bitCheck = 1;
  uint64_t selectedBit = 10000;
  for (int i = 0; i < numCheckpoints; i++) {
    if (!(GBM & bitCheck)) {
      selectedBit = i;
      break;
    } else {
      bitCheck <<= 1;
    }
  }
  assert(selectedBit != 10000);
  // fix gbm - swithced to makeing temp variable

  GBM = GBM | (1 << selectedBit);
  checkpoints save;

  save.GBM = GBM;
  save.FLHead = Freelist.head;
  save.FLHeadPhase = Freelist.headPhase;

  save.SMT = new uint64_t[mapTablesSize];

  for (int i = 0; i < mapTablesSize; i++) {
    save.SMT[i] = RMT[i];
  }

  // BRCheckpoints[selectedBit].FLHead = Freelist.head;
  // BRCheckpoints[selectedBit].FLHeadPhase = Freelist.headPhase;
  // BRCheckpoints[selectedBit].GBM = GBM;

  // for(int i = 0; i < mapTablesSize; i++){
  //	BRCheckpoints[selectedBit].SMT[i] = RMT[i];
  // }
  BRCheckpoints[selectedBit] = save;

  return selectedBit;
}

bool renamer::stall_dispatch(uint64_t bundle_inst) {

  uint64_t numFreeRegs;
  if (ActiveList.head == ActiveList.tail &&
      ActiveList.headPhase == ActiveList.tailPhase) {
    return false;
  }
  if (ActiveList.head == ActiveList.tail &&
      ActiveList.headPhase != ActiveList.tailPhase) {
    return true;
  }

  if (ActiveList.headPhase == ActiveList.tailPhase) {
    numFreeRegs =
        ActiveList.activeListSize - (ActiveList.tail - ActiveList.head);
  }
  if (ActiveList.headPhase != ActiveList.tailPhase) {

    numFreeRegs = ActiveList.head - ActiveList.tail;
  }

  if (numFreeRegs < bundle_inst) {
    return true;
  } else {
    return false;
  }
}

uint64_t renamer::dispatch_inst(bool dest_valid, uint64_t log_reg,
                                uint64_t phys_reg, bool load, bool store,
                                bool branch, bool amo, bool csr, uint64_t PC) {

  assert(!(ActiveList.head == ActiveList.tail &&
           ActiveList.headPhase != ActiveList.tailPhase));

  uint64_t instrIndex;

  instrIndex = ActiveList.tail;
  ActiveList.tail++;
  if (ActiveList.tail == ActiveList.activeListSize) {
    ActiveList.tail = 0;
    ActiveList.tailPhase = !ActiveList.tailPhase;
  }

  ActiveList.active_list[instrIndex].destRegFlag = dest_valid;

  if (dest_valid) {
    ActiveList.active_list[instrIndex].physicalReg = phys_reg;
    ActiveList.active_list[instrIndex].logicalReg = log_reg;
  } else {
    ActiveList.active_list[instrIndex].physicalReg = 10000;
    ActiveList.active_list[instrIndex].logicalReg = 10000;
    // used as invalid nums for now
  }

  ActiveList.active_list[instrIndex].loadFlag = load;
  ActiveList.active_list[instrIndex].storeFlag = store;
  ActiveList.active_list[instrIndex].branchFlag = branch;
  ActiveList.active_list[instrIndex].amoFlag = amo;
  ActiveList.active_list[instrIndex].csrFlag = csr;

  ActiveList.active_list[instrIndex].exception = 0;
  ActiveList.active_list[instrIndex].branchMidprediction = 0;
  ActiveList.active_list[instrIndex].valueMisprediction = 0;
  ActiveList.active_list[instrIndex].loadViolation = 0;
  ActiveList.active_list[instrIndex].completed = 0;
  // clear all exception flags, completed flags

  ActiveList.active_list[instrIndex].PC = PC;

  return instrIndex;
}

bool renamer::is_ready(uint64_t phys_reg) { return (PRFReadyBits[phys_reg]); }

void renamer::clear_ready(uint64_t phys_reg) { PRFReadyBits[phys_reg] = 0; }

uint64_t renamer::read(uint64_t phys_reg) { return (PRF[phys_reg]); }
void renamer::set_ready(uint64_t phys_reg) { PRFReadyBits[phys_reg] = 1; }

void renamer::write(uint64_t phys_reg, uint64_t value) {
  PRF[phys_reg] = value;
}

void renamer::set_complete(uint64_t AL_index) {
  ActiveList.active_list[AL_index].completed = true;
}

void renamer::resolve(uint64_t AL_index, uint64_t branch_ID, bool correct) {

  if (correct) {
    GBM = GBM & ~(1 << branch_ID);

    for (int i = 0; i < numCheckpoints; i++) {
      BRCheckpoints[i].GBM = BRCheckpoints[i].GBM & ~(1 << branch_ID);
    }
  } else {

    GBM = BRCheckpoints[branch_ID].GBM & ~(1 << branch_ID);

    for (int i = 0; i < mapTablesSize; i++) {
      RMT[i] = BRCheckpoints[branch_ID].SMT[i];
    }
    Freelist.head = BRCheckpoints[branch_ID].FLHead;
    Freelist.headPhase = BRCheckpoints[branch_ID].FLHeadPhase;

    ActiveList.tail = AL_index + 1;
    if (ActiveList.tail == ActiveList.activeListSize) {
      ActiveList.tail = 0;
    }

    // check for phase bits

    if (ActiveList.head == ActiveList.tail ||
        ActiveList.head > ActiveList.tail) {
      // active list is full or tail is behind the head which means phase has to
      // be different
      ActiveList.tailPhase = !ActiveList.headPhase;
    } else {
      // tail is in front of head - normal
      ActiveList.tailPhase = ActiveList.headPhase;
    }
  }
}

bool renamer::precommit(bool &completed, bool &exception, bool &load_viol,
                        bool &br_misp, bool &val_misp, bool &load, bool &store,
                        bool &branch, bool &amo, bool &csr, uint64_t &PC) {
  // activelist empty
  if (ActiveList.head == ActiveList.tail &&
      ActiveList.headPhase == ActiveList.tailPhase) {
    return false;
  }

  completed = ActiveList.active_list[ActiveList.head].completed;
  exception = ActiveList.active_list[ActiveList.head].exception;
  load_viol = ActiveList.active_list[ActiveList.head].loadViolation;
  br_misp = ActiveList.active_list[ActiveList.head].branchMidprediction;
  val_misp = ActiveList.active_list[ActiveList.head].valueMisprediction;
  load = ActiveList.active_list[ActiveList.head].loadFlag;
  store = ActiveList.active_list[ActiveList.head].storeFlag;
  branch = ActiveList.active_list[ActiveList.head].branchFlag;
  amo = ActiveList.active_list[ActiveList.head].amoFlag;
  csr = ActiveList.active_list[ActiveList.head].csrFlag;
  PC = ActiveList.active_list[ActiveList.head].PC;

  return true; // instr at head
}

void renamer::commit() {

  assert(!(ActiveList.head == ActiveList.tail &&
           ActiveList.headPhase == ActiveList.tailPhase));
  assert(ActiveList.active_list[ActiveList.head].completed);
  assert(!(ActiveList.active_list[ActiveList.head].exception));
  assert(!(ActiveList.active_list[ActiveList.head].loadViolation));

  uint64_t oldReg;
  if (ActiveList.active_list[ActiveList.head].destRegFlag) {
    // if has dest flag, which some may not, needs to push dest to amt and move
    // old mapping to freelist
    oldReg = AMT[ActiveList.active_list[ActiveList.head].logicalReg];
    AMT[ActiveList.active_list[ActiveList.head].logicalReg] =
        ActiveList.active_list[ActiveList.head].physicalReg;
    // push oldmapping to freelist
    Freelist.listArray[Freelist.tail] = oldReg;
    Freelist.tail++;

    if (Freelist.tail == Freelist.freelistSize) {
      Freelist.tail = 0;
      Freelist.tailPhase = !Freelist.tailPhase;
    }
  }

  // regardless of dest flag, need to retire from active list

  ActiveList.head++;
  if (ActiveList.head == ActiveList.activeListSize) {
    ActiveList.head = 0;
    ActiveList.headPhase = !ActiveList.headPhase;
  }
}

void renamer::squash() {

  // start by flash copying amt to rmt
  for (int i = 0; i < mapTablesSize; i++) {
    RMT[i] = AMT[i];
  }

  // clear AL

  for (int i = 0; i < ActiveList.activeListSize; i++) {
    ActiveList.active_list[i].destRegFlag = 0;
    ActiveList.active_list[i].logicalReg = 0;
    ActiveList.active_list[i].physicalReg = 0;
    ActiveList.active_list[i].completed = 0;
    ActiveList.active_list[i].exception = 0;
    ActiveList.active_list[i].loadViolation = 0;
    ActiveList.active_list[i].branchMidprediction = 0;
    ActiveList.active_list[i].valueMisprediction = 0;
    ActiveList.active_list[i].loadFlag = 0;
    ActiveList.active_list[i].storeFlag = 0;
    ActiveList.active_list[i].branchFlag = 0;
    ActiveList.active_list[i].amoFlag = 0;
    ActiveList.active_list[i].csrFlag = 0;
    ActiveList.active_list[i].PC = 0;
  }

  // rollback tail and copy phase

  ActiveList.tail = ActiveList.head;
  ActiveList.tailPhase = ActiveList.headPhase;

  // restore freelist

  Freelist.head = Freelist.tail;
  Freelist.headPhase = !Freelist.tailPhase;

  // CHECKPOINTs

  GBM = 0;
}

void renamer::set_exception(uint64_t AL_index) {
  ActiveList.active_list[AL_index].exception = true;
}
void renamer::set_load_violation(uint64_t AL_index) {
  ActiveList.active_list[AL_index].loadViolation = true;
}

void renamer::set_branch_misprediction(uint64_t AL_index) {
  ActiveList.active_list[AL_index].branchMidprediction = true;
}
void renamer::set_value_misprediction(uint64_t AL_index) {
  ActiveList.active_list[AL_index].valueMisprediction = true;
}
bool renamer::get_exception(uint64_t AL_index) {
  return ActiveList.active_list[AL_index].exception;
}

uint64_t renamer::getPC() {
  return ActiveList.active_list[ActiveList.head].PC;
}