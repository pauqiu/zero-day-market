#ifndef TRANSFER_TYPE_H
#define TRANSFER_TYPE_H

// How an object changed owner.
enum class TransferType {
    InitialAllocation,  // assigned from the initial inventory to a new user
    Trade,              // exchanged for another object (two transfers, one offer)
    Donation            // given without receiving anything in exchange
};

#endif // TRANSFER_TYPE_H
