*(*channel/new) -> 1.
*(*channel/send! value channel).
*(*channel/receive channel) -> 1.
*(*channel/close!  channel).
*(*channel/empty?  channel) -> 1.
*(*channel/closed? channel) -> 1.
*(*channel/valid?  channel) -> 1.
