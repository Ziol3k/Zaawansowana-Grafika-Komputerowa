FROM python:3.12-slim-bookworm

RUN apt-get update && apt-get install -y \
    build-essential \
    pkg-config \
    libopencv-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY ["projekt 2D/mosaic.cpp", "/build/mosaic.cpp"]

RUN g++ -O2 -std=c++17 \
    /build/mosaic.cpp \
    -o /usr/local/bin/mosaic \
    $(pkg-config --cflags --libs opencv4)

COPY services/mosaic-api/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY services/mosaic-api/app.py .

RUN useradd --create-home appuser
USER appuser

EXPOSE 8000

CMD ["uvicorn", "app:app", "--host", "0.0.0.0", "--port", "8000"]