import { useEffect, useMemo, useRef, useState } from "react";

const HTTP = import.meta.env.VITE_GATEWAY_HTTP ?? "";
const WS = import.meta.env.VITE_GATEWAY_WS ?? `${location.origin.replace("http", "ws")}/ws`;

type Level = { price: string; quantity: string; orderCount: number };
type Order = {
  exchangeOrderId: string;
  clientOrderId: string;
  symbol: number;
  side: number;
  price: string;
  quantity: string;
  remaining: string;
};
type Trade = { tradeId: string; price: string; quantity: string; aggressor: number; symbol: number };

const SYMBOLS = [
  { name: "AAPL", id: 1 },
  { name: "MSFT", id: 2 },
  { name: "GOOG", id: 3 },
];

function sideName(s: number) {
  return s === 0 ? "BUY" : "SELL";
}

export function App() {
  const [token, setToken] = useState(localStorage.getItem("helix_token") ?? "");
  const [user, setUser] = useState("");
  const [status, setStatus] = useState("disconnected");
  const [error, setError] = useState("");
  const [symbol, setSymbol] = useState("AAPL");
  const [side, setSide] = useState("BUY");
  const [tif, setTif] = useState("GTC");
  const [orderType, setOrderType] = useState("LIMIT");
  const [price, setPrice] = useState("100");
  const [qty, setQty] = useState("1");
  const [username, setUsername] = useState("trader1");
  const [password, setPassword] = useState("password1");
  const [orders, setOrders] = useState<Order[]>([]);
  const [bids, setBids] = useState<Level[]>([]);
  const [asks, setAsks] = useState<Level[]>([]);
  const [trades, setTrades] = useState<Trade[]>([]);
  const [modId, setModId] = useState("");
  const [modPx, setModPx] = useState("");
  const [modQty, setModQty] = useState("");
  const wsRef = useRef<WebSocket | null>(null);
  const cid = useRef(1);
  const symbolId = useMemo(() => SYMBOLS.find((s) => s.name === symbol)?.id ?? 1, [symbol]);

  async function auth(path: string) {
    setError("");
    const res = await fetch(`${HTTP}${path}`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ username, password }),
    });
    const body = await res.json();
    if (!res.ok) {
      setError(body.error ?? "auth failed");
      return;
    }
    localStorage.setItem("helix_token", body.token);
    setToken(body.token);
    setUser(body.account.username);
  }

  useEffect(() => {
    if (!token) {
      return;
    }
    const ws = new WebSocket(`${WS}?token=${token}`);
    wsRef.current = ws;
    ws.onopen = () => setStatus("connected");
    ws.onclose = () => setStatus("disconnected");
    ws.onerror = () => setError("websocket error");
    ws.onmessage = (ev) => {
      const msg = JSON.parse(String(ev.data));
      if (msg.type === "engine_status") {
        setStatus(String(msg.status));
        return;
      }
      if (msg.type === "error" || msg.type === "order_rejected") {
        setError(JSON.stringify(msg));
        return;
      }
      setError("");
      if (msg.type === "order_accepted") {
        setOrders((prev) => [
          {
            exchangeOrderId: msg.exchangeOrderId,
            clientOrderId: msg.clientOrderId,
            symbol: msg.symbol,
            side: msg.side,
            price: msg.price,
            quantity: msg.quantity,
            remaining: msg.quantity,
          },
          ...prev,
        ]);
      }
      if (msg.type === "order_canceled" || msg.type === "private_fill") {
        setOrders((prev) =>
          prev
            .map((o) =>
              o.exchangeOrderId === msg.exchangeOrderId
                ? { ...o, remaining: msg.remaining ?? "0" }
                : o,
            )
            .filter((o) => o.remaining !== "0"),
        );
        if (msg.type === "order_canceled") {
          setOrders((prev) => prev.filter((o) => o.exchangeOrderId !== msg.exchangeOrderId));
        }
      }
      if (msg.type === "order_replaced") {
        setOrders((prev) =>
          prev.map((o) =>
            o.exchangeOrderId === msg.exchangeOrderId
              ? { ...o, price: msg.price, remaining: msg.quantity, quantity: msg.quantity }
              : o,
          ),
        );
      }
      if (msg.type === "book_snapshot" && msg.symbol === symbolId) {
        setBids(msg.bids);
        setAsks(msg.asks);
      }
      if (msg.type === "book_delta" && msg.symbol === symbolId) {
        const apply = (levels: Level[]) => {
          const next = levels.filter((l) => l.price !== msg.price);
          if (msg.quantity !== "0") {
            next.push({ price: msg.price, quantity: msg.quantity, orderCount: msg.orderCount });
          }
          next.sort((a, b) => Number(b.price) - Number(a.price));
          return next;
        };
        if (msg.side === 0) {
          setBids((b) => apply(b));
        } else {
          setAsks((a) => apply(a).sort((x, y) => Number(x.price) - Number(y.price)));
        }
      }
      if (msg.type === "public_trade" && msg.symbol === symbolId) {
        setTrades((t) => [msg, ...t].slice(0, 50));
      }
    };
    return () => ws.close();
  }, [token, symbolId]);

  function send(obj: Record<string, unknown>) {
    wsRef.current?.send(JSON.stringify(obj));
  }

  function submitOrder() {
    send({
      type: "new_order",
      clientOrderId: String(cid.current++),
      symbol,
      side,
      orderType,
      tif,
      price,
      quantity: qty,
    });
  }

  if (!token) {
    return (
      <main style={{ maxWidth: 420, margin: "3rem auto" }}>
        <h1>Helix terminal</h1>
        <p>
          <small>Simulated exchange. Not a real market. Not production-ready.</small>
        </p>
        {error && <p className="err">{error}</p>}
        <form
          onSubmit={(e) => {
            e.preventDefault();
            void auth("/api/login");
          }}
        >
          <input value={username} onChange={(e) => setUsername(e.target.value)} placeholder="username" />
          <input
            type="password"
            value={password}
            onChange={(e) => setPassword(e.target.value)}
            placeholder="password"
          />
          <div className="row">
            <button type="submit">Log in</button>
            <button className="secondary" type="button" onClick={() => void auth("/api/register")}>
              Register
            </button>
          </div>
        </form>
      </main>
    );
  }

  return (
    <>
      <header>
        <div>
          <strong>Helix</strong> <small>simulated</small>{" "}
          <span className="badge">{user || "session"}</span>
        </div>
        <div className="row">
          <span className={status === "connected" ? "ok" : "err"}>{status}</span>
          <select value={symbol} onChange={(e) => setSymbol(e.target.value)}>
            {SYMBOLS.map((s) => (
              <option key={s.id}>{s.name}</option>
            ))}
          </select>
          <button
            className="secondary"
            onClick={() => {
              localStorage.removeItem("helix_token");
              setToken("");
            }}
          >
            Log out
          </button>
        </div>
      </header>
      {error && <p className="err" style={{ padding: "0 0.75rem" }}>{error}</p>}
      <div className="layout">
        <section className="panel">
          <h3>Order entry</h3>
          <form
            onSubmit={(e) => {
              e.preventDefault();
              submitOrder();
            }}
          >
            <select value={side} onChange={(e) => setSide(e.target.value)}>
              <option>BUY</option>
              <option>SELL</option>
            </select>
            <select value={orderType} onChange={(e) => setOrderType(e.target.value)}>
              <option>LIMIT</option>
              <option>MARKET</option>
            </select>
            <select value={tif} onChange={(e) => setTif(e.target.value)}>
              <option>GTC</option>
              <option>IOC</option>
              <option>FOK</option>
            </select>
            <input value={price} onChange={(e) => setPrice(e.target.value)} placeholder="price ticks" />
            <input value={qty} onChange={(e) => setQty(e.target.value)} placeholder="quantity" />
            <button type="submit">Submit</button>
          </form>
          <h4>Modify</h4>
          <div className="row">
            <input value={modId} onChange={(e) => setModId(e.target.value)} placeholder="exchange id" />
            <input value={modPx} onChange={(e) => setModPx(e.target.value)} placeholder="price" />
            <input value={modQty} onChange={(e) => setModQty(e.target.value)} placeholder="qty" />
            <button
              type="button"
              onClick={() =>
                send({ type: "modify", exchangeOrderId: modId, price: modPx, quantity: modQty })
              }
            >
              Modify
            </button>
          </div>
        </section>
        <section className="panel">
          <h3>Order book {symbol}</h3>
          <table>
            <thead>
              <tr>
                <th>Bid</th>
                <th>Ask</th>
              </tr>
            </thead>
            <tbody>
              {Array.from({ length: Math.max(bids.length, asks.length, 1) }).map((_, i) => (
                <tr key={i}>
                  <td className="bid">
                    {bids[i] ? `${bids[i].quantity} @ ${bids[i].price}` : ""}
                  </td>
                  <td className="ask">
                    {asks[i] ? `${asks[i].quantity} @ ${asks[i].price}` : ""}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
          <h3>Trades</h3>
          <table>
            <thead>
              <tr>
                <th>Px</th>
                <th>Qty</th>
                <th>Aggressor</th>
              </tr>
            </thead>
            <tbody>
              {trades.map((t) => (
                <tr key={t.tradeId}>
                  <td>{t.price}</td>
                  <td>{t.quantity}</td>
                  <td>{sideName(t.aggressor)}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </section>
        <section className="panel">
          <h3>Open orders</h3>
          <table>
            <thead>
              <tr>
                <th>Id</th>
                <th>Side</th>
                <th>Px</th>
                <th>Rem</th>
                <th></th>
              </tr>
            </thead>
            <tbody>
              {orders
                .filter((o) => o.symbol === symbolId)
                .map((o) => (
                  <tr key={o.exchangeOrderId}>
                    <td>{o.exchangeOrderId}</td>
                    <td>{sideName(o.side)}</td>
                    <td>{o.price}</td>
                    <td>{o.remaining}</td>
                    <td>
                      <button
                        className="secondary"
                        onClick={() => send({ type: "cancel", exchangeOrderId: o.exchangeOrderId })}
                      >
                        Cancel
                      </button>
                    </td>
                  </tr>
                ))}
            </tbody>
          </table>
        </section>
      </div>
    </>
  );
}
