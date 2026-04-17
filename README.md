# power-price-syncronizer
A web service that pulls power priceses fro apis and stores them in to a local db

## Dependencies
```bash
sudo apt install libpq-dev libcurl4-openssl-dev
```

## Setting up a Postgress DB using docker
```bash
docker pull postgres:18

# First time only
docker run --name my-postgres \
  -e POSTGRES_PASSWORD=mysecretpassword \
  -p 5432:5432 \
  -v pgdata:/var/lib/postgresql \
  -d postgres:18

# Subsequent starts
docker start my-postgres
```

