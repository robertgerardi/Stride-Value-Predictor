North Carolina State University - ECE 721 - Advanced Microprocessor Architecture 

**Implemented on top of Eric Rotenberg's RISC-V Pipeline Simulator**

High Level Overview

o The entire implementation consists of the entire pipeline, including stages like fetch, decode, rename, dispatch, issue, execute, writeback, register read, and retire. Every stage, besides the renamer stage and value prediction, was created by our Professor Eric Rotenberg.
o Stride Value Prediction was implemented into the rename, dispatch, writeback, and retire stages to break data dependencies by predicting strided variables.
o The predictor was split up into the SVP Table and VPQ, which allowed for instructions to calculate the next instance/prediction using the number of instances in the VPQ with information from the SVP table.

Competition Phase

o The final project consisted of a competition phase against other students in the class. For the competition, an advanced implementation of value prediction was to be implemented, with the greatest Harmonic Mean IPC increase being the winner.
o Niraj Patel and I worked through multiple implementations, such as RSVP, 2 Delta Strides, and Confidence Changes, but ultimately came to the conclusion that slight confidence changes to the original SVP implementation resulted in the greatest harmonic mean IPC for us. This placed us 5th out of 7 teams.

