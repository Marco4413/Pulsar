// Epoch time (UTC)
*(*time) -> 1.
*(*time/steady) -> 1.
*(*time/micros) -> 1.

*(*print! val).
*(*println! val).

*(get-week-day) -> 1:
  // January 1st 1970 was Thursday
  [ "Thursday", "Friday", "Saturday", "Sunday", "Monday", "Tuesday", "Wednesday" ]
  (*time)  // Milliseconds since Epoch
    1000 / // Seconds
    3600 / // Hours
    24 /   // Days
    7 %
  (!index)
  .

*(main args):
  "Today is "
    (*print!)
  (get-week-day)
    (*println!)
  .
