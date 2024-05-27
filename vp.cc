// VP File
//#include "vp.h"
#include "math.h"
#include "parameters.h"
#include <assert.h>
#include <cstdint>
#include <iostream>
#include <stdlib.h>
#include <cinttypes>
#include "pipeline.h"
#include "payload.h"

using namespace std;

VP::VP() {

  ///////////////////////////////////////////////////
  // STATS
  IneligiblePred = 0;
  EligiblePred = 0;
  VPMiss = 0;
  ConfCorrect = 0;
  ConfIncorrect = 0;
  UnconfCorrect = 0;
  UnconfIncorrect = 0;
  ///////////////////////////////////////////////////

 SVPEntries = pow(2, SVP_INDEX_BITS);

  SVP = new SVPEntry[SVPEntries];

  for (int i = 0; i < SVPEntries; i++) {
    SVP[i].Tag = 0;
    SVP[i].ConfNum = 0;
    SVP[i].RetireVal = 0;
    SVP[i].StrideVal = 0;
    SVP[i].InstanceVal = 0;
  }

  // Add a flag to check if Tag is empty

  VPQ.Counter = 0;
  VPQ.Head = 0;
  VPQ.Tail = 0;
  VPQ.headPhase = false;
  VPQ.tailPhase = false;
  VPQ.VPQEntries = new VPQEntry[VPQ_SIZE];

  for (int i = 0; i < VPQ_SIZE; i++) {
    VPQ.VPQEntries[i].Index = 0; // Might need to change later due to conflicts.
    VPQ.VPQEntries[i].Tag = 0;
    VPQ.VPQEntries[i].PC = 0;
    VPQ.VPQEntries[i].Value = 0;
    VPQ.VPQEntries[i].completed = false;
  }
}

void VP::retire_val(uint64_t PC, uint64_t PAY_VPQIndex) {

  // printf("In Retire\n");

  if (VPQ.Counter == 5600) { // 2013963 is when the instruction messes up //
                             // 2013905 is when the tail gets to 20
                             // printf("FU\n");
  }

  // assert(VPQ.Head == PAY_VPQIndex);
  // cout << PC << endl;
  // cout << VPQ.VPQEntries[VPQ.Head].PC << endl;
  assert(PC == VPQ.VPQEntries[VPQ.Head].PC);
  VPQ.Counter += 1;

  // if (VPQ.VPQEntries[VPQ.Head].completed) {
  //  cout << " completed" << endl;
  //  need to assert VPQ.Head with PAY.buf of VPQIndex;

  VPQEntry currentVPQEntry = VPQ.VPQEntries[VPQ.Head];
  uint64_t currentSVPIndex = currentVPQEntry.Index;
  assert(currentSVPIndex >= 0);
  assert(currentSVPIndex <= pow(2, SVP_INDEX_BITS));
  if (currentVPQEntry.Tag == SVP[currentSVPIndex].Tag || SVP_TAG_BITS == 0) { // Look for TAG not all of PC
    // printf("Training in Retire\n");
    uint64_t new_stride = currentVPQEntry.Value - SVP[currentSVPIndex].RetireVal;
    if (new_stride == SVP[currentSVPIndex].StrideVal) {

      SVP[currentSVPIndex].ConfNum += CONF_INC;

      if (SVP[currentSVPIndex].ConfNum > CONF_MAX) { // "4": Need to set saturation value for predictor
        SVP[currentSVPIndex].ConfNum = CONF_MAX;
      }
    }

    else {

      if (SVP[currentSVPIndex].ConfNum <= REPLACE_STRIDE) {
        SVP[currentSVPIndex].StrideVal = new_stride;

      }
      if (CONF_DEC != 0) {
          for (int i = 0; i < CONF_DEC; i++) {
            if (SVP[currentSVPIndex].ConfNum != 0) {

              SVP[currentSVPIndex].ConfNum -= 1;
            }
          }
        } else {

          SVP[currentSVPIndex].ConfNum = 0;
        }
    }

    SVP[currentSVPIndex].RetireVal = currentVPQEntry.Value;
    SVP[currentSVPIndex].InstanceVal -= 1;
  }

  else if (SVP[currentSVPIndex].ConfNum <= REPLACE_CONF_THRESHOLD) {
    // printf("Replacing in Retire\n");
    uint64_t tempHead = VPQ.Head + 1;
    uint64_t tempTail = VPQ.Tail; // wrong?
    if ((tempHead) == VPQ_SIZE) {
      tempHead = 0;
    }

    assert(currentSVPIndex >= 0);
    assert(currentSVPIndex <= pow(2, SVP_INDEX_BITS));

    if (SVP_TAG_BITS != 0) {
      SVP[currentSVPIndex].Tag = currentVPQEntry.Tag;
    } else {
      SVP[currentSVPIndex].Tag = 0;
    }
    SVP[currentSVPIndex].ConfNum = 0;
    SVP[currentSVPIndex].InstanceVal = 0;
    SVP[currentSVPIndex].RetireVal = currentVPQEntry.Value;
    SVP[currentSVPIndex].StrideVal = currentVPQEntry.Value;

    while (tempHead != tempTail) {
      // cout << "temphead:" << tempHead << endl;
      //  cout << "svp tag: " << SVP[VPQ.VPQEntries[VPQ.Head].Index].Tag <<
      //  endl; cout << "vpq tag: " << VPQ.VPQEntries[tempHead].Tag << endl;
      //  cout << "vpq head index: " << VPQ.VPQEntries[VPQ.Head].Index << endl;
      //  cout << "vpq temp head index: " << VPQ.VPQEntries[tempHead].Index <<
      //  endl;

      if (SVP_TAG_BITS == 0) {
        SVP[currentSVPIndex].InstanceVal += 1;
      } else if (VPQ.VPQEntries[VPQ.Head].Tag == VPQ.VPQEntries[tempHead].Tag &&
                 (VPQ.VPQEntries[VPQ.Head].Index == VPQ.VPQEntries[tempHead].Index) &&
                 SVP_TAG_BITS != 0) {
        // only hits in oracle
        //
        //cout << "c" << endl;
        SVP[currentSVPIndex].InstanceVal += 1;
      }
      tempHead++;
      // cout << "HEAD: " << tempHead << endl;
      // cout << "TAIL: " << tempTail << endl;
      // cout << "SIZE:" << VPQ_SIZE << endl;
      if (tempHead == VPQ_SIZE) {
        tempHead = 0;
      }
    }
  }
  VPQ.Head++;
  if (VPQ.Head == VPQ_SIZE) {
    VPQ.Head = 0;
    VPQ.headPhase = !VPQ.headPhase;
  }
  //}
  // cout << "finish retire" << endl;
  return;
}
uint64_t VP::get_index(uint64_t PC) {
  uint64_t mask = pow(2, SVP_INDEX_BITS) - 1;
  return (mask & (PC >> 2));
}

uint64_t VP::get_tag(uint64_t PC) {
  uint64_t mask = pow(2, SVP_TAG_BITS) - 1;
  return (mask & (PC >> (SVP_INDEX_BITS + 2)));
}

void VP::predict(uint64_t &prediction, bool &confidence, uint64_t &VPQTail,
                 uint64_t PC, bool &SVPHit) {

  // cout << "PREDICT" << endl;
  // uint64_t BitsUnused;
  VPQ.VPQEntries[VPQ.Tail].PC = PC; // should be 95912 at tail = 20.
  assert(!(VPQ.Head == VPQ.Tail && VPQ.headPhase != VPQ.tailPhase));

  assert(VPQ.Tail >= 0);
  assert(VPQ.Tail <= VPQ_SIZE);

  VPQTail = VPQ.Tail;

  // printf("PC Counter: %llx\n", PC);
  VPQ.VPQEntries[VPQ.Tail].Tag = get_tag(PC);
  // printf("Tag: %llx\n", VPQ.VPQEntries[VPQ.Tail].Tag);
  //  Parse through PC to get Tag and Index of PC to put into VPQ
  VPQ.VPQEntries[VPQ.Tail].Index = get_index(PC);
  VPQ.VPQEntries[VPQ.Tail].completed = false;
  // printf("Index: %llx\n", VPQ.VPQEntries[VPQ.Tail].Index);

  assert(VPQ.VPQEntries[VPQ.Tail].Index >= 0);
  assert(VPQ.VPQEntries[VPQ.Tail].Index <= pow(2, SVP_INDEX_BITS));

  if (SVP[VPQ.VPQEntries[VPQ.Tail].Index].Tag == VPQ.VPQEntries[VPQ.Tail].Tag || SVP_TAG_BITS == 0) {
    SVP[VPQ.VPQEntries[VPQ.Tail].Index].InstanceVal += 1;
    SVPHit = true;

  }
  else{
    SVPHit = false;
  }

  /*
    else {
      SVP[newSVPIndex].Tag = newVPQEntry.Tag; // Just in case its not 0
      SVP[newSVPIndex].InstanceVal = 1;
      }
  */
  int tempinstcount = 0;
  int temphead = VPQ.Head;
  int temptail = VPQ.Tail;

  while(temphead != temptail){
      if (SVP_TAG_BITS == 0) {
        tempinstcount++;
      } else if (VPQ.VPQEntries[temptail].Tag == VPQ.VPQEntries[temphead].Tag &&
                 (VPQ.VPQEntries[temptail].Index == VPQ.VPQEntries[temphead].Index) &&
                 SVP_TAG_BITS != 0) {
        // only hits in oracle
        //
        //cout << "c" << endl;
        tempinstcount++;
      }
      temphead++;
      // cout << "HEAD: " << tempHead << endl;
      // cout << "TAIL: " << tempTail << endl;
      // cout << "SIZE:" << VPQ_SIZE << endl;
      if (temphead == VPQ_SIZE) {
        temphead = 0;
      }

  }
  tempinstcount++;
  prediction = SVP[VPQ.VPQEntries[VPQ.Tail].Index].RetireVal + (SVP[VPQ.VPQEntries[VPQ.Tail].Index].InstanceVal * SVP[VPQ.VPQEntries[VPQ.Tail].Index].StrideVal);
 //prediction = SVP[VPQ.VPQEntries[VPQ.Tail].Index].RetireVal + (tempinstcount * SVP[VPQ.VPQEntries[VPQ.Tail].Index].StrideVal);

  // cout << "PREDICT2" << endl;

  assert(VPQ.Head >= 0);
  assert(VPQ.Head <= VPQ_SIZE);

  if (((SVP_TAG_BITS == 0 && SVP[VPQ.VPQEntries[VPQ.Tail].Index].ConfNum == CONF_MAX) || (SVP[VPQ.VPQEntries[VPQ.Tail].Index].Tag == VPQ.VPQEntries[VPQ.Tail].Tag && SVP[VPQ.VPQEntries[VPQ.Tail].Index].ConfNum == CONF_MAX))) {

    confidence = true;
  } else {
    confidence = false;
  }

  VPQ.Tail++;
  if (VPQ.Tail == VPQ_SIZE) {
    VPQ.Tail = 0;
    VPQ.tailPhase = !VPQ.tailPhase;
  }
}

bool VP::stall_VP(unsigned int bundle_dst) {
  uint64_t numAvailableRegs;
  // cout << "STALL" << endl;
  assert(VPQ.Head >= 0);
  assert(VPQ.Head <= VPQ_SIZE);

  assert(VPQ.Tail >= 0);
  assert(VPQ.Tail <= VPQ_SIZE);

  if (VPQ.Head == VPQ.Tail && VPQ.headPhase == VPQ.tailPhase) { // Empty
    return false;
  }
  if (VPQ.Head == VPQ.Tail && VPQ.headPhase != VPQ.tailPhase) { // Full
    return true;
  }
  if (VPQ.headPhase == VPQ.tailPhase) {
    numAvailableRegs = VPQ_SIZE - (VPQ.Tail - VPQ.Head);
  }
  if (VPQ.headPhase != VPQ.tailPhase) {
    numAvailableRegs = VPQ.Head - VPQ.Tail;
  }

  if (numAvailableRegs < bundle_dst) {
    return true;
  } else {
    return false;
  }
}

void VP::set_VPQVal(uint64_t VPQIndex, uint64_t WBVal) {
  //  printf("Set WB Value to VPQ\n");
  assert(VPQIndex >= 0);
  assert(VPQIndex <= VPQ_SIZE);

  VPQ.VPQEntries[VPQIndex].Value = WBVal;
  VPQ.VPQEntries[VPQIndex].completed = true;
  return;
}

void VP::resolveVPQ(uint64_t VPQIndex, bool VPQPhase) {


        instCounterRollback(VPQIndex,VPQPhase);
        VPQ.Tail = VPQIndex;
        VPQ.tailPhase = VPQPhase;



  // Call InstanceFixer Function (backward walk)
}

uint64_t VP::getTail() { return VPQ.Tail; }

bool VP::getPhase() { return VPQ.tailPhase; }

void VP::flush() {
  //  cout << "flush" << endl;
  //instCounterRollback(VPQ.Head);
  // uint64_t tempTail = VPQ.Tail;
  // uint64_t tempHead = VPQ.Head;

  VPQ.Tail = VPQ.Head;
  VPQ.tailPhase = VPQ.headPhase;

  for(int i = 0; i < SVPEntries; i++){
      SVP[i].InstanceVal = 0;
     // cout << "SVP Position: " << i << "  TAG: " << SVP[i].Tag << "   CONF: " << SVP[i].ConfNum << "   RETVAL: " << SVP[i].RetireVal << "   STRIDE: " << SVP[i].StrideVal << "  INST: " << SVP[i].InstanceVal << endl;
  }
  // Call InstanceFixer
  // Function (backward walk)
}

void VP::instCounterRollback(uint64_t VPQIndex, bool VPQPhase) {
  uint64_t VPQTail = 0;
  uint64_t VPQRollBackPoint = 0;


  if (VPQ.Tail == 0) {
    VPQTail = VPQ_SIZE - 1;
  } else {
    VPQTail = VPQ.Tail - 1;
  }

  // might be right - branch gets tail, nothing gets put into vpq after it for
  // some reason, mispredicts and attempts recovery but indices are funky
  VPQRollBackPoint = VPQIndex;

  //cout << "real t: " << VPQTail << endl;
  // cout << "current t: " << VPQ.Tail << endl;
 //cout << "real h: " << VPQ.Head << endl;
  // cout << "roll: " << VPQRollBackPoint << endl;
  // cout << endl;
  // Setting next Index for VPQRollBackPoint
  // assert(VPQTail != VPQRollBackPoint);
  //
if(VPQ.Head == VPQRollBackPoint && VPQ.Tail == VPQ.Head && VPQ.headPhase == VPQ.tailPhase){ //empty return
      return;
  }

 //if((VPQ.Head == VPQRollBackPoint && VPQ.Tail == VPQ.Head && VPQ.headPhase != VPQ.tailPhase) ){
 //     return;
 // }
 if(VPQ.Head == VPQRollBackPoint && (VPQ.Tail == VPQ.Head && VPQ.headPhase != VPQ.tailPhase) && VPQPhase != VPQ.tailPhase){
     for(int i = 0; i < SVPEntries; i++){
         SVP[i].InstanceVal = 0;
        // cout << "SVP Position: " << i << "  TAG: " << SVP[i].Tag << "   CONF: " << SVP[i].ConfNum << "   RETVAL: " << SVP[i].RetireVal << "   STRIDE: " << SVP[i].StrideVal << "  INST: " << SVP[i].InstanceVal << endl;
     }
     return;
 }else if(VPQ.Head == VPQRollBackPoint && (VPQ.Tail == VPQ.Head && VPQ.headPhase != VPQ.tailPhase) && VPQPhase == VPQ.tailPhase){
     return;
 }
  if(VPQ.Tail == VPQRollBackPoint){
      return;
  }
  // I THINK we need this here for branches if nothing is after since they are
    // not put into the VPQ, tested this with perfBP and the asserts in while
    // loop did not fire
 if (VPQTail == VPQRollBackPoint) {
      if (SVP[VPQ.VPQEntries[VPQTail].Index].Tag == VPQ.VPQEntries[VPQTail].Tag) {
        //
       assert((SVP[VPQ.VPQEntries[VPQTail].Index].InstanceVal != 0));

        SVP[VPQ.VPQEntries[VPQTail].Index].InstanceVal -= 1;
        if(VPQTail == VPQ.Head){
             //cout << SVP[VPQ.VPQEntries[VPQTail].Index].InstanceVal << endl;
        }

      }
    }

  while (VPQTail != VPQRollBackPoint) {

    if (SVP[VPQ.VPQEntries[VPQTail].Index].Tag == VPQ.VPQEntries[VPQTail].Tag) {
      //
      assert((SVP[VPQ.VPQEntries[VPQTail].Index].InstanceVal != 0));

      SVP[VPQ.VPQEntries[VPQTail].Index].InstanceVal -= 1;
      // cout << SVP[VPQ.VPQEntries[VPQTail].Index].InstanceVal << endl;
    }

    if (VPQTail == 0) {
      VPQTail = VPQ_SIZE - 1;
    } else {
      VPQTail -= 1;
    }
    if (VPQTail == VPQRollBackPoint) {
      if (SVP[VPQ.VPQEntries[VPQTail].Index].Tag == VPQ.VPQEntries[VPQTail].Tag) {
        //
       assert((SVP[VPQ.VPQEntries[VPQTail].Index].InstanceVal != 0));

        SVP[VPQ.VPQEntries[VPQTail].Index].InstanceVal -= 1;
        if(VPQTail == VPQ.Head){
             //cout << SVP[VPQ.VPQEntries[VPQTail].Index].InstanceVal << endl;
        }

      }
    }
  }
}

void VP::dump_stats(FILE* fp){
  int TotalInstructions = IneligiblePred + EligiblePred;
  fprintf(fp, "VPU MEASUREMENTS-----------------------------------\n");

  fprintf(fp, "vpmeas_ineligible         :\t%d (%.2f%%)  // Not eligible for value prediction.\n",IneligiblePred, 100.0*(double)IneligiblePred/(double)TotalInstructions);
  fprintf(fp, "   vpmeas_ineligible_type :\t%d (%.2f%%)  // Not eligible for value prediction.\n",IneligibleType, 100.0*(double)IneligibleType/(double)TotalInstructions);
  fprintf(fp, "   vpmeas_ineligible_drop :\t%d (%.2f%%)  // Not eligible for value prediction.\n\n",IneligibleDrop, 100.0*(double)IneligibleDrop/(double)TotalInstructions);
  fprintf(fp, "vpmeas_eligible           :\t%d (%.2f%%)  // Eligible for value prediction.\n", EligiblePred, 100.0*(double)EligiblePred/(double)TotalInstructions);
  fprintf(fp, "   vpmeas_miss            :\t%d (%.2f%%)  // VPU was unable to generate a value prediction (e.g., SVP miss).\n",VPMiss, 100.0*(double)VPMiss/(double)TotalInstructions);
  fprintf(fp, "   vpmeas_conf_corr       :\t%d (%.2f%%)  // VPU generated a confident and correct value prediction.\n",ConfCorrect, 100.0*(double)ConfCorrect/(double)TotalInstructions);
  fprintf(fp, "   vpmeas_conf_incorr     :\t%d (%.2f%%)  // VPU generated a confident and incorrect value prediction. (MISPREDICTION)\n",ConfIncorrect, 100.0*(double)ConfIncorrect/(double)TotalInstructions);
  fprintf(fp, "   vpmeas_unconf_corr     :\t%d (%.2f%%)  // VPU generated an unconfident and correct value prediction. (LOST OPPORTUNITY)\n",UnconfCorrect, 100.0*(double)UnconfCorrect/(double)TotalInstructions);
  fprintf(fp, "   vpmeas_unconf_incorr   :\t%d (%.2f%%)  // VPU generated an unconfident and incorrect value prediction.\n",UnconfIncorrect, 100.0*(double)UnconfIncorrect/(double)TotalInstructions);
/*
  printf("VPU MEASUREMENTS-----------------------------------\n");

  printf("vpmeas_ineligible         :\t%d (%.2f%%)  // Not eligible for value prediction.\n",IneligiblePred, 100.0*(double)IneligiblePred/(double)TotalInstructions);
  printf("   vpmeas_ineligible_type :\t%d (%.2f%%)  // Not eligible for value prediction.\n",IneligibleType, 100.0*(double)IneligibleType/(double)TotalInstructions);
  printf("   vpmeas_ineligible_drop :\t%d (%.2f%%)  // Not eligible for value prediction.\n\n",IneligibleDrop, 100.0*(double)IneligibleDrop/(double)TotalInstructions);
  printf("vpmeas_eligible           :\t%d (%.2f%%)  // Eligible for value prediction.\n", EligiblePred, 100.0*(double)EligiblePred/(double)TotalInstructions);
  printf("   vpmeas_miss            :\t%d (%.2f%%)  // VPU was unable to generate a value prediction (e.g., SVP miss).\n",VPMiss, 100.0*(double)VPMiss/(double)TotalInstructions);
  printf("   vpmeas_conf_corr       :\t%d (%.2f%%)  // VPU generated a confident and correct value prediction.\n",ConfCorrect, 100.0*(double)ConfCorrect/(double)TotalInstructions);
  printf("   vpmeas_conf_incorr     :\t%d (%.2f%%)  // VPU generated a confident and incorrect value prediction. (MISPREDICTION)\n",ConfIncorrect, 100.0*(double)ConfIncorrect/(double)TotalInstructions);
  printf("   vpmeas_unconf_corr     :\t%d (%.2f%%)  // VPU generated an unconfident and correct value prediction. (LOST OPPORTUNITY)\n",UnconfCorrect, 100.0*(double)UnconfCorrect/(double)TotalInstructions);
  printf("   vpmeas_unconf_incorr   :\t%d (%.2f%%)  // VPU generated an unconfident and incorrect value prediction.\n",UnconfIncorrect, 100.0*(double)UnconfIncorrect/(double)TotalInstructions);
*/
}

int VP::num_entries(){
    int numAvailableRegs;
    if (VPQ.headPhase == VPQ.tailPhase) {
      numAvailableRegs = VPQ_SIZE - (VPQ.Tail - VPQ.Head);
    }
    if (VPQ.headPhase != VPQ.tailPhase) {
      numAvailableRegs = VPQ.Head - VPQ.Tail;
    }
    return numAvailableRegs;
}
