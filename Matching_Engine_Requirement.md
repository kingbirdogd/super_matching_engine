# Matching Engine Requirement

## Overview

A matching engine processes order requests to facilitate trading on an exchange, matching buy and sell orders to execute trades.

Your task is to develop an application that processes a sequence of order requests (add or remove), executes matching logic, and outputs messages detailing resulting trades and order state changes.

This document includes:

- matching logic
- message formats (input/output)
- assignment requirements
- end-to-end example input/output

## Logic

Orders represent offers to buy or sell a specified quantity at a certain price.

- Buy orders target lower prices.
- Sell orders target higher prices.
- A trade occurs when buy price $\ge$ sell price.

An incoming order that causes a price cross is an **aggressive order**. Orders already in the book are **resting orders**.

Resting order matching priority:

1. Best price for the aggressive side first
2. Oldest order first at the same price

Trades occur at the resting order's price and for the smaller quantity of the two orders.

If an aggressive order is not fully filled, it continues matching in price-then-age order until:

- it is fully filled, or
- no further crossing order exists.

Any remaining quantity becomes a new resting order.

### Book Example

Resting orders (oldest to newest; B = buy, S = sell):

```text
1075 S 1
1050 S 10
1025 S 2, 5
1000 B 9, 1
975  B 30
```

Best buy is 1000 and best sell is 1025, so no match occurs.

If a new buy order arrives for quantity 3 at price 1050:

- It can match sell orders at 1050 or below.
- Better sell price 1025 matches first.
- Older order at 1025 (qty 2) fills first.
- Remaining qty 1 fills against the next 1025 order (qty 5), leaving qty 4.

Generated events:

- Trade: 2 @ 1025
- Trade: 1 @ 1025
- One order fully removed (qty 2 order)
- One order partially reduced (qty 5 -> qty 4)

Updated resting orders:

```text
1075 S 1
1050 S 10
1025 S 4
1000 B 9,1
975  B 30
```

If a new sell order arrives at 1025, it is queued behind the existing 1025 sell order.

## Messages

There are five message types total:

- `0`: AddOrderRequest (input)
- `1`: CancelOrderRequest (input)
- `2`: TradeEvent (output)
- `3`: OrderFullyFilled (output)
- `4`: OrderPartiallyFilled (output)

## Input

### AddOrderRequest

Format:

```text
msgtype,orderid,side,quantity,price
```

Example:

```text
0,123,0,9,1000
```

Fields:

- `msgtype`: `0`
- `orderid`: unique positive integer
- `side`: `0` = Buy, `1` = Sell
- `quantity`: positive integer
- `price`: decimal number

### CancelOrderRequest

Format:

```text
msgtype,orderid
```

Example:

```text
1,123
```

Fields:

- `msgtype`: `1`
- `orderid`: order ID to remove

## Output

### TradeEvent

Format:

```text
msgtype,quantity,price
```

Example:

```text
2,2,1025
```

Notes:

- `msgtype`: `2`
- one TradeEvent per matched order pair

### OrderFullyFilled

Format:

```text
msgtype,orderid
```

Example:

```text
3,123
```

Notes:

- `msgtype`: `3`
- emitted when an order is fully consumed and removed

### OrderPartiallyFilled

Format:

```text
msgtype,orderid,quantity
```

Example:

```text
4,123,3
```

Notes:

- `msgtype`: `4`
- `quantity` is the new remaining quantity

## Assignment Requirements

Implement an in-memory order book from a stream of input messages read from `stdin`.

When an aggressive order matches, write resulting trade and order update messages to `stdout`.

For each matched pair, emit in order:

1. `TradeEvent`
2. aggressive order fill status (`OrderFullyFilled` or `OrderPartiallyFilled`)
3. resting order fill status (`OrderFullyFilled` or `OrderPartiallyFilled`)

Technical constraints:

- C++14 or higher
- Linux platform
- no third-party runtime libraries in core implementation
- third-party test frameworks are allowed

Quality expectations:

- clean, readable, robust, efficient code
- clear build/run instructions
- no crash on malformed input
- clear error logging to `stderr`
- include datasets and test/supporting code

Performance discussion required for:

- determining if AddOrderRequest results in a match
- removing filled orders
- removing canceled orders

Also describe production-level improvements (optimization, memory/layout, architecture, scalability) and any performance trade-offs.

## Example

Comments are explanatory only and are not part of actual input.

Input (`stdin`):

```text
0,1000000,1,1,1075
0,1000001,0,9,1000
0,1000002,0,30,975
0,1000003,1,10,1050
0,1000004,0,10,950
BADMESSAGE
0,1000005,1,2,1025
0,1000006,0,1,1000
1,1000004
0,1000007,1,5,1025
0,1000008,0,3,1050
```

Output (`stdout`) caused by final message:

```text
2,2,1025
4,1000008,1
3,1000005
2,1,1025
3,1000008
4,1000007,4
```

Possible error output (`stderr`) for bad input:

```text
Unknown message type: BADMESSAGE
```
