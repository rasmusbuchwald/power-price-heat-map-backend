# power-price-heat-map Backend
A web service that pulls power priceses and populates them as a heat map

## Dependencies
```bash
sudo apt install libpq-dev libcurl4-openssl-dev
```

## Setting up a Postgress DB using docker
```bash
docker pull postgres:18

docker run --name my-postgres \
  -e POSTGRES_PASSWORD=mysecretpassword \
  -p 5432:5432 \
  -v pgdata:/var/lib/postgresql \
  -d postgres:18
```

