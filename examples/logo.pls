*(*println! val).

*(list-println list):
  <- list (!length) if 0: .
  (!head) (*println!)
  (list-println)
  .

*(main args):
  [
    "       3",
    "     -44444454",
    "    !44331  2445555:",
    "   343  33c       45",
    "  541    22",
    ",55c     =21  44443",
    "=5.       ?1!_22222",
    "           11111111",
    "            100000?",
    "            :?????b",
    ";33221       ?!!!!_       ;22334",
    "43221100??.   ?!!!    b??01122333",
    " !22110??!!!!+_!!  a!!!!?001123-",
    "       b?!!!!!!!!!!!!!!!?-",
    "              !!!!a",
    "           =!!!, !!!!",
    "         :???!;  _!!???",
    "        11000a    b?0001?",
    "      a22211a      0111222",
    "     443332,        1223334a",
    "     04443          +134444,",
    "        b            ?23:      ,5-",
    "                      23=     a55.",
    "                       33    255",
    "              53       b43  553",
    "              ;5555542  34455!",
    "                    55555555,",
    "                          4",
    "       Welcome to Pulsar!",
  ] (list-println)
  .
