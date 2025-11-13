# Sammich

Sammich is an interpreter that sits between the Creality 4.2.7 board and our feeders and vacuum. It's purporse is to interpret the simpler I2C commands coming from the Creality board into a more complex format for us to use. This board instructs the feeders how far they should advance depending on the loaded component and receives data back from their encoders. A future goal is to enable an automatic addressing for the feeders which would allow for plug and play compatibility.
