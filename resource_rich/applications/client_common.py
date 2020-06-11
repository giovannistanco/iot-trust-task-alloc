import logging
import asyncio
import signal

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("app-client")
logger.setLevel(logging.DEBUG)

edge_server_port = 10_000

application_serial_prefix = "!e"

class Client:
    def __init__(self, name):
        self.name = name
        self.reader = None
        self.writer = None

    async def receive(self, message):
        raise NotImplementedError()

    async def start(self):
        self.reader, self.writer = await asyncio.open_connection('localhost', edge_server_port)

        # Need to inform bridge of what application we represent
        self.writer.write(f"{self.name}\n")
        await self.writer.drain()

        # Once started, we need to inform the edge of this application's availability
        self._inform_application_started()

    async def run(self):
        async for line in self.reader.stdout:
            line = line.decode("utf-8").strip()

            await self.receive(line)

    async def stop(self):
        # When stopping, we need to inform the edge that this application is no longer available
        self._inform_application_stopped()

        self.writer.close()
        await self.writer.wait_closed()

        self.reader = None
        self.writer = None

    async def _inform_application_started(self):
        self.writer.write(f"{application_serial_prefix}{self.name}:start\n")
        await self.writer.drain()

    async def _inform_application_stopped(self):
        self.writer.write(f"{application_serial_prefix}{self.name}:stop\n")
        await self.writer.drain()


async def do_run(service)
    await service.start()
    await service.run()

async def shutdown(signal, loop, services):
    """Cleanup tasks tied to the service's shutdown."""
    logger.info(f"Received exit signal {signal.name}...")

    logger.info(f"Stopping services tasks...")
    await asyncio.gather([service.stop() for service in services], return_exceptions=True)

    tasks = [t for t in asyncio.all_tasks() if t is not asyncio.current_task()]
    for task in tasks:
        task.cancel()

    logger.info(f"Cancelling {len(tasks)} outstanding tasks...")
    await asyncio.gather(*tasks, return_exceptions=True)
    logger.info(f"Finished cancelling tasks!")

    loop.stop()

def main(name, services):
    logger.info(f"Starting {name} application")

    loop = asyncio.get_event_loop()

    # May want to catch other signals too
    signals = (signal.SIGHUP, signal.SIGTERM, signal.SIGINT)
    for sig in signals:
        loop.add_signal_handler(sig, lambda sig=sig: asyncio.create_task(shutdown(sig, loop, services)))

    try:
        for service in services:
            loop.create_task(do_run(service))
        loop.run_forever()
    finally:
        loop.close()
        logger.info(f"Successfully shutdown the {name} application.")