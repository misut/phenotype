#include <string>
import phenotype;

using namespace phenotype;

auto CounterApp() {
    auto count = encode(0);

    Column([count] {
        Text("Hello, phenotype!");
        Text(std::string("Count: ") + std::to_string(count->value()));

        Row(
            [count] { Button("-", [count] { count->set(count->value() - 1); }); },
            [count] { Button("+", [count] { count->set(count->value() + 1); }); }
        );
    });
}

int main() {
    express(CounterApp);
    return 0;
}
