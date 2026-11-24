from io import BytesIO
from pathlib import Path
from tempfile import TemporaryDirectory
import asyncio
import subprocess

from fastapi import FastAPI, File, HTTPException, UploadFile
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import Response
from PIL import Image, UnidentifiedImageError

MAX_UPLOAD_BYTES = 8 * 1024 * 1024
MAX_PIXELS = 3_000_000
PROCESS_TIMEOUT = 60

app = FastAPI(title="Mosaic Generator API")

app.add_middleware(
    CORSMiddleware,
    allow_origins=[
        "http://localhost:5173",
        "http://127.0.0.1:5173",
    ],
    allow_methods=["GET", "POST"],
    allow_headers=["*"],
)

processing_slots = asyncio.Semaphore(2)

@app.get("/health")
def health() -> dict[str, str]:
    return {"status": "ok"}

def generate_mosaic(data: bytes, extension: str) -> bytes:
    with TemporaryDirectory(prefix="mosaic-") as tmp:
        workdir = Path(tmp)

        input_path = workdir / f"input.{extension}"
        output_path = workdir / "output.png"

        input_path.write_bytes(data)

        try:
            completed = subprocess.run(
                [
                    "/usr/local/bin/mosaic",
                    str(input_path),
                    str(output_path),
                ],
                capture_output=True,
                text=True,
                timeout=PROCESS_TIMEOUT,
                check=False,
            )
        except subprocess.TimeoutExpired:
            raise HTTPException(
                status_code=504,
                detail="Przetwarzanie trwało zbyt długo",
            )

        if completed.returncode != 0 or not output_path.is_file():
            raise HTTPException(
                status_code=500,
                detail="Nie udało się wygenerować mozaiki",
            )

        return output_path.read_bytes()

@app.post("/api/mosaic")
async def create_mosaic(
    file: UploadFile = File(...),
) -> Response:
    try:
        data = await file.read(MAX_UPLOAD_BYTES + 1)
    finally:
        await file.close()

    if len(data) > MAX_UPLOAD_BYTES:
        raise HTTPException(
            status_code=413,
            detail="Maksymalny rozmiar pliku to 8 MB",
        )

    try:
        with Image.open(BytesIO(data)) as image:
            
            if image.width * image.height > 20_000_000:
                raise HTTPException(
                    status_code=413,
                    detail="Obraz może mieć maksymalnie 20 megapikseli",
                )

            extension = "jpg" if image.format == "JPEG" else "png"

            if image.width * image.height > MAX_PIXELS:
                image.thumbnail(
                    (1600, 1600),
                    Image.Resampling.LANCZOS
                )

                rgb = Image.new("RGB", image.size, "white")

                if "A" in image.getbands():
                    rgba = image.convert("RGBA")
                    rgb.paste(rgba, mask=rgba.getchannel("A"))
                else:
                    rgb.paste(image.convert("RGB"))

                buffer = BytesIO()
                rgb.save(buffer, format="JPEG", quality=92)

                data = buffer.getvalue()
                extension = "jpg"
            else:
                image.verify()


    except (UnidentifiedImageError, OSError, ValueError):
        raise HTTPException(
            status_code=422,
            detail="Nieprawidłowy plik obrazu",
        )

    async with processing_slots:
        result = await asyncio.to_thread(
            generate_mosaic,
            data,
            extension,
        )

    return Response(
        content=result,
        media_type="image/png",
        headers={
            "Content-Disposition":
                'attachment; filename="mosaic.png"'
        },
    )