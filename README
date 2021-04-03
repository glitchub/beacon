Usage:

    beacon send [-N] interface [message]

Send ethernet beacons on the specified interface, forever until killed.

Where:
    
    "-N" is the repeat rate in seconds, default 1.

    "message" is up to 1498 characters to be sent in the beacon payload. The
    default message is "beacon". 

-or-

    beacon recv [-N] [regex]

Wait for an ethernet beacon on any interface, print the contained message and
exit 0, or exit 1 on timeout.

Where:

    -N is the number of seconds to wait, default 5. If 0 then never timeout,
    just print received beacons forever.

    "regex" is a regular expression to match in the beacon message. Non
    matching beacons are ignored.  Only the part of the message that matches
    the regex will be printed (case-insensitive). The default regex is ".*$",
    i.e. match any beacon.
