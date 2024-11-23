#pragma once
constexpr char MAIN_HTML[] = R"html(
<!doctype html>
<html lang="en-US">
  <head>
    <meta charset="utf-8" />
    <title>My test page</title>
    <script>
      function getSavesAndDetails(callback)
      {
          const DBOpenRequest = window.indexedDB.open("degrees-of-lewdity");
          DBOpenRequest.onerror = (event) => {
              console.error("Error loading database.");
          };

          DBOpenRequest.onsuccess = (event) => {
              console.info("Database initialized.");

              let result = {};
              // store the result of opening the database in the db
              // variable. This is used a lot later on, for opening
              // transactions and suchlike.
              db = DBOpenRequest.result;

              console.log(db.objectStoreNames);
              const trans = db.transaction(["saves", "details"], "readonly");
              const store = trans.objectStore("saves");
              const req = store.getAll();
              req.onerror = (event) => {
                  console.error("Failed to read saves.");
              };

              req.onsuccess = (event) => {
                  result["saves"] = req.result;

                  const store1 = trans.objectStore("details");
                  const req1 = store1.getAll();
                  req1.onerror = (event) => {
                      console.error("Failed to read details.");
                  };

                  req1.onsuccess = (event) => {
                      result["details"] = req1.result;
                      callback(result);
                  };
              }
          };
      }

      function setSavesAndDetails(data)
      {
          const DBOpenRequest = window.indexedDB.open("degrees-of-lewdity");
          DBOpenRequest.onerror = (event) => {
              console.error("Error loading database.");
          };

		  DBOpenRequest.onupgradeneeded = event => {
			  console.log("updating idb", event.oldVersion);
			  switch (event.oldVersion) {
              case 0:
              db.createObjectStore("saves", { keyPath: "slot" });
			  db.createObjectStore("details", { keyPath: "slot" });
              }
          };

          DBOpenRequest.onsuccess = (event) => {
              console.info("Database initialized.");
              // store the result of opening the database in the db
              // variable. This is used a lot later on, for opening
              // transactions and suchlike.
              db = DBOpenRequest.result;

              console.log(db.objectStoreNames);
              const trans = db.transaction(["saves", "details"], "readwrite");
              const store = trans.objectStore("saves");
              const req_clear = store.clear();
              req_clear.onsuccess = (event) => {
                  for(let i = 0; i < data["saves"].length; i++)
                  {
                      const req = store.add(data.saves[i]);
                      req.onerror = (event) => {
                          console.error("Failed to store saves.");
                      };
                  }
              };
              const store1 = trans.objectStore("details");
              const req_clear1 = store1.clear();
              req_clear1.onsuccess = (event) => {
                  for(let i = 0; i < data["details"].length; i++)
                  {
                      const req = store1.add(data.details[i]);
                      req.onerror = (event) => {
                          console.error("Failed to store details:");
                          console.error(req.error);
                      };
                  }
              };
          };
      }

      function getLocalStorage()
      {
          let result = {};
          for(let i = 0; i < window.localStorage.length; i++)
          {
              const key = window.localStorage.key(i);
              const value = window.localStorage.getItem(key);
              result[key] = value;
          }
          return result;
      }

      function setLocalStorage(data)
      {
          for(let [key, value] of Object.entries(data))
          {
              window.localStorage.setItem(key, value);
          }
      }

      function getSessionStorage()
      {
          let result = {};
          for(let i = 0; i < window.sessionStorage.length; i++)
          {
              const key = window.sessionStorage.key(i);
              const value = window.sessionStorage.getItem(key);
              result[key] = value;
          }
          return result;
      }

      function setSessionStorage(data)
      {
          for(let [key, value] of Object.entries(data))
          {
              window.sessionStorage.setItem(key, value);
          }
      }
    </script>
  </head>
  <body>
    <header>
      <div>
        <a class="Button" id="BtnLoad" href="#">Load from Server</a>
        <a class="Button" id="BtnSave" href="#">Save to Server</a>
      </div>
    </header>
    <iframe
      id="DoL"
      title="DoL"
      width="100%"
      height="1000"
      src="{{ url_for("dol", "Degrees of Lewdity.html") }}">
    </iframe>
    <script>
      document.getElementById("BtnLoad").addEventListener("click", (event) => {
          fetch("{{ url_for("retrieve-save", "") }}").then((res) => {
              if(res.status == 200)
              {
                  return res.json();
              }
              else
              {
                  console.error("Failed to retrieve save");
              }
          }).then((data) => {
              setSavesAndDetails(data.db);
              setLocalStorage(data.local);
              setSessionStorage(data.session);
          });
      });
      document.getElementById("BtnSave").addEventListener("click", (event) => {
          getSavesAndDetails((saves) => {
              const local_data = getLocalStorage();
              const session_data = getSessionStorage();
              fetch("{{ url_for("store-save", "") }}", {
                  method: "POST",
                  body: JSON.stringify({"db": saves, "local": local_data, "session": session_data})
              }).then((res) => {
                  if(res.status != 200)
                  {
                      console.error("Failed to store save");
                  }
              });
          });
      });
    </script>
  </body>
</html>
)html";
