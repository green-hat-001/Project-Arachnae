import asyncio, websockets, json
async def main():
    async with websockets.connect("ws://<xiao-ip>:8765") as ws:
        async for msg in ws:
            d = json.loads(msg)
            if d.get("type") == "log":
                print(d["msg"])
asyncio.run(main())