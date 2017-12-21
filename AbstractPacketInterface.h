#ifndef ABSTRACTPACKETINTERFACE_H
#define ABSTRACTPACKETINTERFACE_H

#include <stdint.h>

/**
 * The Abstract Packet Interface allows sending segmented (arbitrarily fragmented data)
 * to a receiver to be treated as a single packet. The transaction may be cancelled if
 * a segment is lost for example.
 */
class AbstractPacketInterface {
public:
    /**
     * Start a transaction, must be called before transmitData()
     * @return true if successful
     */
    virtual bool startTransaction() = 0;

    /**
     * Transmit a segment of the packet, must be called after startTransaction()
     * @param data bytes to transmit
     * @param length number of bytes
     * @return true if successful (can be retried if not)
     */
    virtual bool transmitData(const uint8_t* data, uint16_t length) = 0;

    /**
     * Commit the transaction: finish packet after all of its segments have been sent
     * @note commit or cancel must be called before starting another transaction
     * @return true if successful (can be retried if not)
     */
    virtual bool commitTransaction() = 0;

    /**
     * Cancel current transaction
     * @returns void, resets the interface to its default state (must not fail)
     */
    virtual void cancelTransaction() = 0;

    virtual ~AbstractPacketInterface() {}
};

#endif // ABSTRACTPACKETINTERFACE_H
