// parking_system.cpp
// Versión optimizada del sistema de estacionamiento
// Compatibilidad: Windows (WinAPI), compilar con Visual Studio (MSVC)

#include <iostream>
#include <conio.h>
#include <string>
#include <windows.h>
#include <vector>
#include <ctime>
#include <limits>
#include <iomanip>

using namespace std;

struct Ticket {
    string id = "";
    int hora = 0;
    int min = 0;
    int dia = 0;
    int yyyy = 0;
    int mes = 0;
    int lugar = 0;
    string placa = "";
    float cobro = 0;
    bool activo = true;
};

vector<Ticket> RegistroTickets;
const int totalLugares = 6;
vector<string> lugaresOcupados(totalLugares); 
const float pagoHora = 20.00;
Ticket boletoSalida;
int contadorTickets = 0;
int numeroLugar = 0;


// --------------------------- SerialController -------------------------------
class SerialController {
private:
    HANDLE hSerial = INVALID_HANDLE_VALUE;
    bool connected = false;
    string accion;
    bool newDataAvailable = false;  // Nueva bandera para controlar datos nuevos

    int safeStoi(const string& str, int defaultValue = -1) {
        if (str.empty()) return defaultValue;
        for (char c : str) if (!isdigit((unsigned char)c)) return defaultValue;
        try { return stoi(str); } catch (...) { return defaultValue; }
    }

public:
    SerialController() = default;

    bool connect(const char* portName) {
        cout << "Intentando conectar a " << portName << "..." << endl;

        hSerial = CreateFileA(portName,
                             GENERIC_READ | GENERIC_WRITE,
                             0,
                             NULL,
                             OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL,
                             NULL);

        if (hSerial == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            cout << "Error al conectar a " << portName << " (Codigo: " << error << ")" << endl;
            if (error == 5) cout << "Acceso denegado. Cierra el Monitor Serial del IDE Arduino." << endl;
            else if (error == 2) cout << "Puerto no encontrado." << endl;
            return false;
        }

        DCB dcbSerialParams = {0};
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

        if (!GetCommState(hSerial, &dcbSerialParams)) {
            cout << "Error: No se pudo obtener configuracion del puerto" << endl;
            CloseHandle(hSerial);
            hSerial = INVALID_HANDLE_VALUE;
            return false;
        }

        dcbSerialParams.BaudRate = CBR_9600;
        dcbSerialParams.ByteSize = 8;
        dcbSerialParams.StopBits = ONESTOPBIT;
        dcbSerialParams.Parity   = NOPARITY;

        if (!SetCommState(hSerial, &dcbSerialParams)) {
            cout << "Error: No se pudo configurar el puerto serial" << endl;
            CloseHandle(hSerial);
            hSerial = INVALID_HANDLE_VALUE;
            return false;
        }

        COMMTIMEOUTS timeouts = {0};
        timeouts.ReadIntervalTimeout = 50;
        timeouts.ReadTotalTimeoutConstant = 50;
        timeouts.ReadTotalTimeoutMultiplier = 10;
        timeouts.WriteTotalTimeoutConstant = 50;
        timeouts.WriteTotalTimeoutMultiplier = 10;

        if (!SetCommTimeouts(hSerial, &timeouts)) {
            cout << "Error: No se pudo configurar timeouts" << endl;
            CloseHandle(hSerial);
            hSerial = INVALID_HANDLE_VALUE;
            return false;
        }

        connected = true;
        cout << "Conectado al puerto " << portName << " correctamente!" << endl;
        clearSerialBuffer();
        return true;
    }

    void clearSerialBuffer() {
        if (!connected || hSerial == INVALID_HANDLE_VALUE) return;
        DWORD errors = 0;
        COMSTAT comStat = {0};
        ClearCommError(hSerial, &errors, &comStat);
        if (comStat.cbInQue > 0) {
            vector<char> tmp(comStat.cbInQue + 1);
            DWORD read = 0;
            ReadFile(hSerial, tmp.data(), (DWORD)tmp.size() - 1, &read, NULL);
            // Se ignora el contenido; objetivo: limpiar buffer
        }
    }

    bool sendData(const string& data) {
        if (!connected || hSerial == INVALID_HANDLE_VALUE) return false;
        DWORD bytesWritten = 0;
        string dataWithNewline = data + "\n";
        bool success = WriteFile(hSerial, dataWithNewline.c_str(),
                                 (DWORD)dataWithNewline.length(), &bytesWritten, NULL);
        if (!success) {
            cout << "Error al enviar datos" << endl;
        }
        return success;
    }

    // Devuelve true si hay nuevos datos disponibles
    bool hasNewData() const { 
        return newDataAvailable; 
    }

    // Obtiene el último dato y marca como consumido
    string getLastData() {
        newDataAvailable = false;  // Resetear después de obtener
        return accion;
    }

    // Lee datos del puerto y actualiza 'accion' cuando haya algo nuevo
    void checkForData() {
        if (!connected || hSerial == INVALID_HANDLE_VALUE) return;

        char buffer[256];
        DWORD bytesRead = 0;
        BOOL ok = ReadFile(hSerial, buffer, sizeof(buffer) - 1, &bytesRead, NULL);
        if (ok && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            string receivedData(buffer);
            
            // DEBUG: Ver datos crudos
            cout << "DEBUG: Bytes leidos: " << bytesRead << endl;
            cout << "DEBUG: Raw data: '";
            for(int i = 0; i < bytesRead; i++) {
                if(buffer[i] == '\r') cout << "\\r";
                else if(buffer[i] == '\n') cout << "\\n";
                else cout << buffer[i];
            }
            cout << "'" << endl;
            
            // Limpiar saltos de línea al final
            while (!receivedData.empty() && (receivedData.back() == '\r' || receivedData.back() == '\n')) {
                receivedData.pop_back();
            }
            
            // Solo marcar como nuevo dato si es diferente al anterior
            if (!receivedData.empty()) {
                cout << "DEBUG: Datos limpios '" <<  receivedData << "'" << endl;
                accion = receivedData;
                newDataAvailable = true;  // Marcar que hay nuevo dato
                cout << "DEBUG: Nuevo dato disponible: '" << accion << "'" << endl;
            }
        }
    }

    ~SerialController() {
        if (connected && hSerial != INVALID_HANDLE_VALUE) {
            CloseHandle(hSerial);
            hSerial = INVALID_HANDLE_VALUE;
            connected = false;
            cout << "Conexion serial cerrada." << endl;
        }
    }
};

// --------------------------- Administracion de lugares  ------------------------------

// Función para encontrar el primer lugar disponible
int encontrarLugarDisponible() {
    for (int i = 0; i < totalLugares; i++) {
        cout << "Lugar A - " << i + 1 << endl;
        if (lugaresOcupados[i] == "" ) {
            cout << "Lugar A - " << i + 1 << endl;
            return i;  // Devuelve el índice del lugar disponible
        }
    }
    return -1;  // No hay lugares disponibles
}

// Función para contar lugares ocupados
int contarLugaresOcupados() {
    int count = 0;
    for (int i = 0; i < totalLugares; i++) {
        if (lugaresOcupados[i] == "" ) count++;
    }
    return count;
}

// --------------------------- Menús y utilidades ------------------------------
void consultarTicket() {
    string ticket;
    
    cout << "\n     =================================" << endl;
    cout << "     ===     Consultar Ticket      ===" << endl;
    cout << "     =================================" << endl;
    cout << "\n      Teclear numero de Ticket " << endl;
    cout << "    (Presione enter al terminar): ";
    cin >> ticket;
    cout << "\n\n------------------------------------------" << endl;
    cout << "------------------------------------------" << endl;

    bool found = false;
    for (const auto& registro : RegistroTickets) {
        if (registro.id == ticket) {
            found = true;
            cout << "\n    ID:  " << registro.id    << endl;
            cout << " Lugar:  " << registro.lugar << endl;
            cout << "  Hora:  " << registro.hora << ":" << registro.min << endl;
            cout << " Fecha:  " << registro.dia << "/" << registro.mes << "/" << registro.yyyy << endl;
            boletoSalida = registro; 
        }
    }

    if (!found) {
        cout << "Ticket no encontrado." << endl;
        boletoSalida.id = "";
    }
    cout << "------------------------------------------" << endl;
    cout << "Presione enter para continuar..." << endl;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cin.get();
}

float pagoTotal() {
    int lugarIndex = 0; // aqui2
    ostringstream wnum, wmes, wdia, whora, wmin;
    ostringstream wn, wmm, wdd, whh, wmn;
    bool found = false;
    char op;

    string ticket;

    float pagoDD = 0;
    float pagoHH = 0;
    float pagoMin = 0;
    float TotalxCobrar = 0;


    cout << "\n     =================================" << endl;
    cout << "     ===   Informacion del Ticket   ===" << endl;
    cout << "     =================================" << endl;
    cout << "\n      Teclear numero de Ticket " << endl;
    cout << "    (Presione enter al terminar): ";
    cin >> ticket;
    cout << "\n\n------------------------------------------" << endl;
    cout << "------------------------------------------" << endl;

    for (const auto& registro : RegistroTickets) {
        if (registro.id == ticket) {
            found = true;
            cout << "\n    ID:  " << registro.id    << endl;
            cout << " Lugar:  " << registro.lugar << endl;
            cout << "  Hora:  " << registro.hora << ":" << registro.min << endl;
            cout << " Fecha:  " << registro.dia << "/" << registro.mes << "/" << registro.yyyy << endl;
            boletoSalida = registro; 
        }
    }
    
    if (!found) {
        cout << "Ticket no encontrado."  << boletoSalida.id << endl;
        boletoSalida.id = "";
        return 0;
    }

    do {
        cout << "------------------------------------------" << endl;
        cout << "Desea continuar [s/n]?  ";
        cin.get(op);

        op = tolower(op);

        if (op == 'n') {
            cout << "------------------------------------------" << endl;
            cout << "Cancelando el proceso de salida...  " << endl;
            Sleep(500);
            return 0;
        }
    } while (op != 's' && op != 'n');

    //Borrando boleto de los cajones de estacionamiento
    for (int i = 0; i < 6 ; i++) {
        if (lugaresOcupados[i] == boletoSalida.id) {
            lugaresOcupados [i] = "";
            lugarIndex = i;
            found = true;
        }
    }
    
    // hola1
    wmes << setw(2) << setfill('0') << boletoSalida.mes;
    wdia << setw(2) << setfill('0') << boletoSalida.dia;
    wmin << setw(2) << setfill('0') << boletoSalida.min;
    whora << setw(2) << setfill('0') << boletoSalida.hora;

    time_t now = time(nullptr);
    tm* tiempo_desglosado = localtime(&now);
    int yy = tiempo_desglosado->tm_year + 1900;
    int mm = tiempo_desglosado->tm_mon + 1;
    int dd = tiempo_desglosado->tm_mday;
    int hh = tiempo_desglosado->tm_hour;
    int min = tiempo_desglosado->tm_min;
// wn, wmm, wdd, whh, wmn
    wmm << setw(2) << setfill('0') << mm;
    wdd << setw(2) << setfill('0') << dd;
    whh << setw(2) << setfill('0') << hh;
    wmn << setw(2) << setfill('0') << min;

    cout << "\n =====================================" << endl;
    cout << "===     Ticket: " << boletoSalida.id << "      ===" << endl;
    cout << " =====================================" << endl;
    cout << " Lugar:  " << "A-" << boletoSalida.lugar << endl;
    cout << " Fecha:  " << wdia.str() << "/" << wmes.str() << "/" << boletoSalida.yyyy << endl;
    cout << "  Hora:  " << whora.str() <<":" << wmin.str() << endl;
    cout << "\n =====================================" << endl;
    cout << "datos actuales: " << endl;
    cout << "Fecha Salida : " << wdd.str() << "/" <<  wmm.str() << "/" << yy <<  endl;
    cout << "Hora Salida  : " << whh.str() << ":" << wmn.str() << endl;

    if (yy == boletoSalida.yyyy && boletoSalida.activo) {
        if (mm == boletoSalida.mes) {

            if (dd == boletoSalida.dia) {
                if (hh > boletoSalida.hora) {
                    pagoHH = (hh - boletoSalida.hora) * pagoHora;
                }
            } else {
                if (dd > boletoSalida.dia && (dd - boletoSalida.dia > 1)) {
                    pagoDD = ((dd -boletoSalida.dia) - 1) * 24;
                    cout << "Total de horas  : " << pagoDD << endl;
                    pagoDD = (pagoDD + hh) * pagoHora;
                    cout << "Total de horas:  $  " << pagoDD << endl;
                    pagoMin = min * (pagoHora/60);
                } else {
                    pagoDD = (24 - boletoSalida.hora); 
                    //pagoDD = pagoDD + (dd - boletoSalida.dia);
                    pagoDD = (pagoDD + hh) * pagoHora;
                    pagoMin = (min + boletoSalida.min)* (pagoHora / 60);
                }
            }
        }
    }
    
    cout << "Lugar A-" << boletoSalida.lugar << " liberado." << endl;
    cout << "\nLugares disponibles: " << (totalLugares - contarLugaresOcupados()) << endl;

    TotalxCobrar = pagoDD + pagoHH + pagoMin;

    cout << "Por favor, reciba:  $ ";
    cout << fixed << setprecision(2) << TotalxCobrar;
    cout << "  del cliente. " << endl;

    // desactivar el boleto que sale pero dejarlo en el sistema para consultas
    for (int i = 0; i < RegistroTickets.size(); i++) {
        if (RegistroTickets[i].id == boletoSalida.id) {
            RegistroTickets[i].activo = false;
            RegistroTickets[i].cobro = TotalxCobrar;
            lugarIndex = boletoSalida.lugar;
            boletoSalida.id = "";
            boletoSalida.hora = 0;
            boletoSalida.min = 0;
            boletoSalida.dia = 0;
            boletoSalida.yyyy = 0;
            boletoSalida.mes = 0;
            boletoSalida.lugar = 0;
            boletoSalida.placa = "";
            boletoSalida.cobro = 0;
            boletoSalida.activo = true;
        }
    }    

    cout << "\n------------------------------------------" << endl;
    cout << "Presione enter para continuar..." << endl;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cin.get();
    return 0;
}

void listarLugares() {
    for (int i = 0; i < 6; i++) {
        cout << lugaresOcupados[i] << endl;
    }
    cout << "\n------------------------------------------" << endl;
    cout << "Presione enter para continuar..." << endl;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cin.get();
}

void listarTickets() {
    ostringstream wid, wreg, wdd, wmm, whh, wmin;
    string estado = "activo";

    system("cls");
    cout << "\n======================================================" << endl;
    cout << "\n                  Tickets registrados                 " << endl;
    cout << "\n------------------------------------------------------" << endl;
    cout << "\n       ID        Lugar    Estado     Fecha      Hora  " << endl;
    cout << "\n======================================================" << endl;
    cout << " " << endl;

    if (RegistroTickets.empty()) {
        cout << " No hay tickets registrados." << endl;
    } else { //aqui1
        for (const auto& registro : RegistroTickets) {
            wreg << setw(2) << setfill('0') << "A-" << registro.lugar;
            wdd << setw(2) << setfill('0') << registro.dia;
            wmm << setw(2) << setfill('0') << registro.mes;
            whh << setw(2) << setfill('0') << registro.hora;
            wmin << setw(2) << setfill('0') << registro.min;
            if (!registro.activo) {
                estado = "No Activo";
            } else {
                estado = "Activo";
            }

            cout << registro.id <<"    " <<  wreg.str() << "     " <<  estado;
            cout << "   " << wdd.str() << "/" << wmm.str() << "/" << registro.yyyy;
            cout << "   " << whh.str() << ":" << wmin.str() << endl;
            wreg.str("");
            wid.str("");
            wreg.str("");
            wdd.str("");
            wmm.str("");
            whh.str("");
            wmin.str("");
        }
    }

    cout << "\n======================================================" << endl;
    cout << "\n   Presione enter para continuar..." << endl;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cin.get();
}

void menu() {
    int op = -1;
    while (op != 0) {
        system("cls");
        cout << "\n=================================" << endl;
        cout << "\n       Consulta de tickets       " << endl;
        cout << "\n=================================" << endl;
        cout << "\n  1) Consulta ticket por numero  " << endl;
        cout << "\n  2) Listar todos los tickets    " << endl;
        cout << "\n  3) Listar lugares              " << endl;
        cout << "\n  0) Salir                       " << endl;
        cout << "\n" << endl;
        cout << "\n=================================" << endl;
        cout << "\n  Teclee una opcion: ";

        if (!(cin >> op)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            op = -1;
            continue;
        }

        switch (op) {
            case 1: consultarTicket(); break;
            case 2: listarTickets(); break;
            case 3: listarLugares(); break;
            case 0: break;
            default: cout << "Opcion invalida." << endl; Sleep(500); break;
        }
    }
}

void cargaPrevia(){
    //                             id            hr  min dia yy    mm lug  placa   cobro activo
    RegistroTickets.push_back({"TCK-2025110001", 10, 30, 15, 2025, 11, 1, "ABC123", 0, true});
    RegistroTickets.push_back({"TCK-2025110002", 11, 15, 20, 2025, 11, 2, "DEF456", 0, true});
    RegistroTickets.push_back({"TCK-2025110003", 12, 0, 20, 2025, 11, 3, "GHI789", 0, true});

    lugaresOcupados[0] = "TCK-2025110001";
    lugaresOcupados[1] = "TCK-2025110002";
    lugaresOcupados[2] = "TCK-2025110003";

    contadorTickets = 3;
}

// --------------------------- Función principal ------------------------------
int main() {
    SerialController controller;
    int accionRsp = 0;
    int pos = -1;
    string wnum;
    string puerto;

    // Intentar varios puertos
    const char* puertos[] = { "COM3", "COM4", "COM5", "COM6", "COM7", "COM8" };
    bool conectado = false;
    for (const char* p : puertos) {
        if (controller.connect(p)) {
            puerto = p;
            conectado = true;
            break;
        }
    }

    cargaPrevia();

    if (!conectado) {
        cout << "\nSugerencias para solucionar el problema:" << endl;
        cout << "1. Cierra el Monitor Serial del IDE Arduino" << endl;
        cout << "2. Verifica que el Arduino este conectado por USB" << endl;
        cout << "3. Prueba reiniciar el Arduino" << endl;
        cout << "4. Verifica en el Administrador de Dispositivos" << endl;
    }

    // Estado para mostrar pantalla solo cuando cambie
    bool mostrarPantallaEspera = true;

    while (true) {
        controller.checkForData();
        
        if (controller.hasNewData()) {
            cout << "DEBUG: Entrando en if - Nuevos datos detectados!" << endl;
            string datoRecibido = controller.getLastData();
            cout << "DEBUG: Dato recibido procesado: '" << datoRecibido << "'" << endl;
            
            // Si llega nuevo dato, permitimos re-dibujar la pantalla de espera
            mostrarPantallaEspera = true;

            // buscar primer numero (soporta que vengan letras antes)
            pos = (int)datoRecibido.find_first_of("0123456789");
            if (pos >= 0) {
                // tomamos hasta 3 dígitos por si acaso (ajustar según protocolo)
                int len = 2;
                if ((int)datoRecibido.size() - pos < len) len = (int)datoRecibido.size() - pos;
                wnum = datoRecibido.substr(pos, len);
                int val = -1;
                try { val = stoi(wnum); } catch(...) { val = -1; }
                accionRsp = val;

                if (accionRsp >= 0) {
                    switch (accionRsp) {
                        case 40: {
                            
                            int lugarIndex = encontrarLugarDisponible();
                            cout << "lugar --- " << lugarIndex << endl;

                            if (lugarIndex != -1) {

                                 // MARCAR EL LUGAR COMO OCUPADO
                                numeroLugar = lugarIndex + 1;  // Para mostrar A-1, A-2, etc.

                                ostringstream wnum, wmes, wdia, whora, wmin;

                                time_t now = time(nullptr);
                                tm* tiempo_desglosado = localtime(&now);
                                int yy = tiempo_desglosado->tm_year + 1900;

                                int mm = tiempo_desglosado->tm_mon + 1;
                                wmes << setw(2) << setfill('0') << mm;

                                int dd = tiempo_desglosado->tm_mday;
                                wdia << setw(2) << setfill('0') << dd;

                                int hh = tiempo_desglosado->tm_hour;
                                whora << setw(2) << setfill('0') << hh;

                                int min = tiempo_desglosado->tm_min;
                                wmin << setw(2) << setfill('0') << min;

                                contadorTickets++;
                                wnum << setw(4) << setfill('0') << contadorTickets;

                                system("cls");
                                cout << "\n\nRecibiendo auto... " << endl;
                                cout << "\n  " << wdia.str() << "/" << wmes.str() << "/" << yy << "   ----   " << whora.str() << ":" << wmin.str() << endl;
                                cout << "\n--------------------------------" << endl;
                                cout << "     Entrada de vehiculo\n" << endl;
                                cout << "\n     Generando ticket... " << endl;

                                Ticket nuevoTicket;
                                
                                nuevoTicket.id = "TCK-" + to_string(yy) + wmes.str() +  wnum.str();
                                nuevoTicket.lugar = numeroLugar;
                                nuevoTicket.hora = hh;
                                nuevoTicket.min = min;
                                nuevoTicket.dia = dd;
                                nuevoTicket.mes = mm;
                                nuevoTicket.yyyy = yy;
                                nuevoTicket.placa = "ABC1234";
                                lugaresOcupados[lugarIndex] = nuevoTicket.id;

                                RegistroTickets.push_back(nuevoTicket);

                                cout << "\n     =================================" << endl;
                                cout << "\n     === TICKET DE ESTACIONAMIENTO ===" << endl;
                                cout << "\n     =================================" << endl;
                                cout << "      Ticket: # " << nuevoTicket.id << "\n" << endl;
                                cout << "      Fecha Actual: " << wdia.str() << "/" << wmes.str() << "/" << yy << endl;
                                cout << "      Hora  Actual: " << whora.str() << ":" << wmin.str() << endl;
                                cout << "      Lugar: " << "A-" << nuevoTicket.lugar << endl;
                                cout << "\n     =================================" << endl;
                                cout << "\nPresione <F2> para acceder al Menu" << endl;
                                Sleep(300);
                                controller.sendData("1");
                                controller.clearSerialBuffer();
                            } else {
                                cout << "No hay lugares!!!" << endl;
                                controller.sendData("0"); // Por ejemplo, 0 = lleno
                                controller.clearSerialBuffer();
                            }
                            break;
                        }
                        case 30: {
                                if (contarLugaresOcupados() > 0) {
                                    system("cls");
                                    cout << "\n     ===    SALIDA DE VEHICULO     ===" << endl;
                                    int cobrar = pagoTotal();
                                    if (cobrar == 0) { 
                                        controller.sendData("0");
                                        controller.clearSerialBuffer();
                                        break;
                                    }
                                    //cout << "Vehiculo salio... " << endl;
                                    //salida
                                    cout << "Lugares disponibles: " << ((totalLugares - numeroLugar) + 1)  << endl;
                                    Sleep(300);
                                    controller.sendData("2");
                                    controller.clearSerialBuffer();
                                } else {
                                    cout << "No hay autos en el estacionamiento.\n "<< endl;
                                    controller.sendData("0");
                                    controller.clearSerialBuffer();
                                }
                                break;
                            }
                        default: {
                            // otros códigos pueden manejarse aquí
                            cout << "DEBUG: Codigo no manejado: " << accionRsp << endl;
                            break;
                        }
                        } // switch
                } // accionRsp
            } // pos
        } else {
            if (mostrarPantallaEspera) {
                system("cls");
                cout << "\n\n\n" << endl;
                cout << "=== SISTEMA DE ESTACIONAMIENTO ===" << endl;
                cout << "\n-------------------------------------" << endl;
                cout << " Para acceder al Menu presione    F2 " << endl;
                cout << " Para salir del programa presione F10" << endl;
                cout << "-------------------------------------" << endl;
                mostrarPantallaEspera = false;
            }
        }

        // Manejo de teclado no bloqueante: detectar F2 y F10
        if (_kbhit()) {
            int t = _getch();
            if (t == 0 || t == 224) {
                int key = _getch();
                // F2: codigo 60 en tu sistema anterior, mantengo comprobación por si responde así
                if (key == 60 || key == 59 /* alternativa */) {
                    menu();
                    mostrarPantallaEspera = true; // Redibujar después del menú
                }
                // F10: en la mayoría de consoles Windows llega como 68 (pero puede variar).
                if (key == 68) {
                    cout << "Saliendo..." << endl;
                    break;
                }
                // Algunas consolas devuelven 133 para F10; se puede extender si es necesario.
            } else {
                // Si la tecla no es extendida, revisar si es ESC (27) para salir rápido
                if (t == 27) { // ESC
                    cout << "Saliendo..." << endl;
                    break;
                }
            }
        }

        Sleep(100); // dormir 100 ms por ciclo
    } // while

    return 0;
}