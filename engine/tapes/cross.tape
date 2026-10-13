# Deterministic crossing tape used as a golden-file fixture.
# Sell 5, buy 3 (partial), cancel remaining 2.
SYMBOL 1 TEST
NEW 1 1 1 SELL LIMIT GTC 100 5
NEW 2 1 1 BUY LIMIT GTC 100 3
CANCEL 1 1
