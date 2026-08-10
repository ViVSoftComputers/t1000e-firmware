// Updated for v24 firmware — event_state bits mapped to scan types.
// Upload this in ChirpStack > Device profiles > SenseCAP Card Tracker T1000-E > Codec.

function getEventStatus (str) {
    let bitStr = getByteArray(str)
    let bitArr = []
    for (let i = 0; i < bitStr.length; i++) {
        bitArr[i] = bitStr.substring(i, i + 1)
    }
    bitArr = bitArr.reverse()
    let event = []
    for (let i = 0; i < bitArr.length; i++) {
        if (bitArr[i] !== '1') {
            continue
        }
        switch (i){
            case 0:
                event.push({id:1, eventName:"Power-on scan event"})
                break
            case 1:
                // Unused in v24 — kept for backward compat
                event.push({id:2, eventName:"End movement event."})
                break
            case 2:
                // Unused in v24 — kept for backward compat
                event.push({id:3, eventName:"Motionless event."})
                break
            case 3:
                // Unused in v24 — kept for backward compat
                event.push({id:4, eventName:"Shock event."})
                break
            case 4:
                // Unused in v24 — kept for backward compat
                event.push({id:5, eventName:"Temperature event."})
                break
            case 5:
                // Unused in v24 — kept for backward compat
                event.push({id:6, eventName:"Light event."})
                break
            case 6:
                // Unused in v24 — kept for backward compat
                event.push({id:7, eventName:"SOS event."})
                break
            case 7:
                event.push({id:8, eventName:"Point of Interest scan event"})
                break
        }
    }
    // If no event bits are set, this is a scheduled/periodic or turbo scan.
    // The firmware clears event_state after every scan, so scheduled scans
    // arrive with event_state=0x00.
    if (event.length === 0) {
        event.push({id:0, eventName:"Scheduled scan event"})
    }
    return event
}
