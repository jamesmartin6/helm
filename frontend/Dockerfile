FROM node:22-slim AS build

WORKDIR /app
COPY package.json package-lock.json ./
RUN npm ci

COPY . .

# Baked in at build time: the browser (not another container) is what
# actually calls this URL, so it needs to be host-reachable -- matches the
# backend port docker-compose.yml maps to the host.
ENV VITE_API_BASE_URL=http://localhost:8000
RUN npm run build

FROM nginx:alpine

COPY --from=build /app/dist /usr/share/nginx/html

EXPOSE 80
